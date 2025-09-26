# Copyright 2023 The Bazel Authors. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"Python toolchain module extensions for use with bzlmod."

load("@bazel_features//:features.bzl", "bazel_features")
load("//python:versions.bzl", "DEFAULT_RELEASE_BASE_URL", "PLATFORMS", "TOOL_VERSIONS")
load(":auth.bzl", "AUTH_ATTRS")
load(":full_version.bzl", "full_version")
load(":platform_info.bzl", "platform_info")
load(":python_register_toolchains.bzl", "python_register_toolchains")
load(":pythons_hub.bzl", "hub_repo")
load(":repo_utils.bzl", "repo_utils")
load(
    ":toolchains_repo.bzl",
    "host_compatible_python_repo",
    "multi_toolchain_aliases",
    "sorted_host_platform_names",
    "sorted_host_platforms",
)
load(":util.bzl", "IS_BAZEL_6_4_OR_HIGHER")
load(":version.bzl", "version")

def parse_modules(*, module_ctx, logger, _fail = fail):
    """Parse the modules and return a struct for registrations.

    Args:
        module_ctx: {type}`module_ctx` module context.
        logger: {type}`repo_utils.logger` A logger to use.
        _fail: {type}`function` the failure function, mainly for testing.

    Returns:
        A struct with the following attributes:
        * `toolchains`: {type}`list[ToolchainConfig]` The list of toolchains to
          register. The last element is special and is treated as the default
          toolchain.
        * `config`: Various toolchain config, see `_get_toolchain_config`.
        * `debug_info`: {type}`None | dict` extra information to be passed
          to the debug repo.
        * `platforms`: {type}`dict[str, platform_info]` of the base set of
          platforms toolchains should be created for, if possible.

        ToolchainConfig struct:
            * python_version: str, full python version string
            * name: str, the base toolchain name, e.g., "python_3_10", no
              platform suffix.
            * register_coverage_tool: bool
    """
    if module_ctx.os.environ.get("RULES_PYTHON_BZLMOD_DEBUG", "0") == "1":
        debug_info = {
            "toolchains_registered": [],
        }
    else:
        debug_info = None

    # The toolchain_info structs to register, in the order to register them in.
    # NOTE: The last element is special: it is treated as the default toolchain,
    # so there is special handling to ensure the last entry is the correct one.
    toolchains = []

    # We store the default toolchain separately to ensure it is the last
    # toolchain added to toolchains.
    # This is a toolchain_info struct.
    default_toolchain = None

    # Map of string Major.Minor or Major.Minor.Patch to the toolchain_info struct
    global_toolchain_versions = {}

    ignore_root_user_error = None

    # if the root module does not register any toolchain then the
    # ignore_root_user_error takes its default value: True
    if not module_ctx.modules[0].tags.toolchain:
        ignore_root_user_error = True

    config = _get_toolchain_config(modules = module_ctx.modules, _fail = _fail)

    default_python_version = None
    for mod in module_ctx.modules:
        defaults_attr_structs = _create_defaults_attr_structs(mod = mod)
        default_python_version_env = None
        default_python_version_file = None

        # Only the root module and rules_python are allowed to specify the default
        # toolchain for a couple reasons:
        # * It prevents submodules from specifying different defaults and only
        #   one of them winning.
        # * rules_python needs to set a soft default in case the root module doesn't,
        #   e.g. if the root module doesn't use Python itself.
        # * The root module is allowed to override the rules_python default.
        if mod.is_root or (mod.name == "rules_python" and not default_python_version):
            for defaults_attr in defaults_attr_structs:
                default_python_version = _one_or_the_same(
                    default_python_version,
                    defaults_attr.python_version,
                    onerror = _fail_multiple_defaults_python_version,
                )
                default_python_version_env = _one_or_the_same(
                    default_python_version_env,
                    defaults_attr.python_version_env,
                    onerror = _fail_multiple_defaults_python_version_env,
                )
                default_python_version_file = _one_or_the_same(
                    default_python_version_file,
                    defaults_attr.python_version_file,
                    onerror = _fail_multiple_defaults_python_version_file,
                )
            if default_python_version_file:
                default_python_version = _one_or_the_same(
                    default_python_version,
                    module_ctx.read(default_python_version_file, watch = "yes").strip(),
                )
            if default_python_version_env:
                default_python_version = module_ctx.getenv(
                    default_python_version_env,
                    default_python_version,
                )

    seen_versions = {}
    for mod in module_ctx.modules:
        module_toolchain_versions = []
        toolchain_attr_structs = _create_toolchain_attr_structs(
            mod = mod,
            seen_versions = seen_versions,
            config = config,
        )

        for toolchain_attr in toolchain_attr_structs:
            toolchain_version = toolchain_attr.python_version
            toolchain_name = "python_" + toolchain_version.replace(".", "_")

            # Duplicate versions within a module indicate a misconfigured module.
            if toolchain_version in module_toolchain_versions:
                _fail_duplicate_module_toolchain_version(toolchain_version, mod.name)
            module_toolchain_versions.append(toolchain_version)

            if mod.is_root:
                # Only the root module and rules_python are allowed to specify the default
                # toolchain for a couple reasons:
                # * It prevents submodules from specifying different defaults and only
                #   one of them winning.
                # * rules_python needs to set a soft default in case the root module doesn't,
                #   e.g. if the root module doesn't use Python itself.
                # * The root module is allowed to override the rules_python default.
                if default_python_version:
                    is_default = default_python_version == toolchain_version
                    if toolchain_attr.is_default and not is_default:
                        fail("The 'is_default' attribute doesn't work if you set " +
                             "the default Python version with the `defaults` tag.")
                else:
                    is_default = toolchain_attr.is_default

                # Also only the root module should be able to decide ignore_root_user_error.
                # Modules being depended upon don't know the final environment, so they aren't
                # in the right position to know or decide what the correct setting is.

                # If an inconsistency in the ignore_root_user_error among multiple toolchains is detected, fail.
                if ignore_root_user_error != None and toolchain_attr.ignore_root_user_error != ignore_root_user_error:
                    fail("Toolchains in the root module must have consistent 'ignore_root_user_error' attributes")

                ignore_root_user_error = toolchain_attr.ignore_root_user_error
            elif mod.name == "rules_python" and not default_toolchain and not default_python_version:
                # We don't do the len() check because we want the default that rules_python
                # sets to be clearly visible.
                is_default = toolchain_attr.is_default
            else:
                is_default = False

            if is_default and default_toolchain != None:
                _fail_multiple_default_toolchains(
                    first = default_toolchain.name,
                    second = toolchain_name,
                )

            # Ignore version collisions in the global scope because there isn't
            # much else that can be done. Modules don't know and can't control
            # what other modules do, so the first in the dependency graph wins.
            if toolchain_version in global_toolchain_versions:
                # If the python version is explicitly provided by the root
                # module, they should not be warned for choosing the same
                # version that rules_python provides as default.
                first = global_toolchain_versions[toolchain_version]
                if mod.name != "rules_python" or not first.module.is_root:
                    # The warning can be enabled by setting the verbosity:
                    # env RULES_PYTHON_REPO_DEBUG_VERBOSITY=INFO bazel build //...
                    _warn_duplicate_global_toolchain_version(
                        toolchain_version,
                        first = first,
                        second_toolchain_name = toolchain_name,
                        second_module_name = mod.name,
                        logger = logger,
                    )
                toolchain_info = None
            else:
                toolchain_info = struct(
                    python_version = toolchain_attr.python_version,
                    name = toolchain_name,
                    register_coverage_tool = toolchain_attr.configure_coverage_tool,
                    module = struct(name = mod.name, is_root = mod.is_root),
                )
                global_toolchain_versions[toolchain_version] = toolchain_info
                if debug_info:
                    debug_info["toolchains_registered"].append({
                        "ignore_root_user_error": ignore_root_user_error,
                        "module": {"is_root": mod.is_root, "name": mod.name},
                        "name": toolchain_name,
                    })

            if is_default:
                # This toolchain is setting the default, but the actual
                # registration was performed previously, by a different module.
                if toolchain_info == None:
                    default_toolchain = global_toolchain_versions[toolchain_version]

                    # Remove it because later code will add it at the end to
                    # ensure it is last in the list.
                    toolchains.remove(default_toolchain)
                else:
                    default_toolchain = toolchain_info
            elif toolchain_info:
                toolchains.append(toolchain_info)

    config.default.setdefault("ignore_root_user_error", ignore_root_user_error)

    # A default toolchain is required so that the non-version-specific rules
    # are able to match a toolchain.
    if default_toolchain == None:
        fail("No default Python toolchain configured. Is rules_python missing `python.defaults()`?")
    elif default_toolchain.python_version not in global_toolchain_versions:
        fail('Default version "{python_version}" selected by module ' +
             '"{module_name}", but no toolchain with that version registered'.format(
                 python_version = default_toolchain.python_version,
                 module_name = default_toolchain.module.name,
             ))

    # The last toolchain in the BUILD file is set as the default
    # toolchain. We need the default last.
    toolchains.append(default_toolchain)

    # sort the toolchains so that the toolchain versions that are in the
    # `minor_mapping` are coming first. This ensures that `python_version =
    # "3.X"` transitions work as expected.
    minor_version_toolchains = []
    other_toolchains = []
    minor_mapping = list(config.minor_mapping.values())
    for t in toolchains:
        # FIXME @aignas 2025-04-04: How can we unit test that this ordering is
        # consistent with what would actually work?
        if config.minor_mapping.get(t.python_version, t.python_version) in minor_mapping:
            minor_version_toolchains.append(t)
        else:
            other_toolchains.append(t)
    toolchains = minor_version_toolchains + other_toolchains

    return struct(
        config = config,
        debug_info = debug_info,
        default_python_version = default_toolchain.python_version,
        toolchains = [
            struct(
                python_version = t.python_version,
                name = t.name,
                register_coverage_tool = t.register_coverage_tool,
            )
            for t in toolchains
        ],
    )

def _python_impl(module_ctx):
    logger = repo_utils.logger(module_ctx, "python")
    py = parse_modules(module_ctx = module_ctx, logger = logger)

    # Host compatible runtime repos
    # dict[str version, struct] where struct has:
    # * full_python_version: str
    # * platform: platform_info struct
    # * platform_name: str platform name
    # * impl_repo_name: str repo name of the runtime's python_repository() repo
    all_host_compatible_impls = {}

    # Host compatible repos that still need to be created because, when
    # creating the actual runtime repo, there wasn't a host-compatible
    # variant defined for it.
    # dict[str reponame, struct] where struct has:
    # * compatible_version: str, e.g. 3.10 or 3.10.1. The version the host
    #   repo should be compatible with
    # * full_python_version: str, e.g. 3.10.1, the full python version of
    #   the toolchain that still needs a host repo created.
    needed_host_repos = {}

    # list of structs; see inline struct call within the loop below.
    toolchain_impls = []

    # list[str] of the repo names for host compatible repos
    all_host_compatible_repo_names = []

    # Create the underlying python_repository repos that contain the
    # python runtimes and their toolchain implementation definitions.
    for i, toolchain_info in enumerate(py.toolchains):
        is_last = (i + 1) == len(py.toolchains)

        # Ensure that we pass the full version here.
        full_python_version = full_version(
            version = toolchain_info.python_version,
            minor_mapping = py.config.minor_mapping,
        )
        kwargs = {
            "python_version": full_python_version,
            "register_coverage_tool": toolchain_info.register_coverage_tool,
        }

        # Allow overrides per python version
        kwargs.update(py.config.kwargs.get(toolchain_info.python_version, {}))
        kwargs.update(py.config.kwargs.get(full_python_version, {}))
        kwargs.update(py.config.default)
        register_result = python_register_toolchains(
            name = toolchain_info.name,
            _internal_bzlmod_toolchain_call = True,
            **kwargs
        )
        if not register_result.impl_repos:
            continue

        host_platforms = {}
        for repo_name, (platform_name, platform_info) in register_result.impl_repos.items():
            toolchain_impls.append(struct(
                # str: The base name to use for the toolchain() target
                name = repo_name,
                # str: The repo name the toolchain() target points to.
                impl_repo_name = repo_name,
                # str: platform key in the passed-in platforms dict
                platform_name = platform_name,
                # struct: platform_info() struct
                platform = platform_info,
                # str: Major.Minor.Micro python version
                full_python_version = full_python_version,
                # bool: whether to implicitly add the python version constraint
                # to the toolchain's target_settings.
                # The last toolchain is the default; it can't have version constraints
                set_python_version_constraint = is_last,
            ))
            if _is_compatible_with_host(module_ctx, platform_info):
                host_compat_entry = struct(
                    full_python_version = full_python_version,
                    platform = platform_info,
                    platform_name = platform_name,
                    impl_repo_name = repo_name,
                )
                host_platforms[platform_name] = host_compat_entry
                all_host_compatible_impls.setdefault(full_python_version, []).append(
                    host_compat_entry,
                )
                parsed_version = version.parse(full_python_version)
                all_host_compatible_impls.setdefault(
                    "{}.{}".format(*parsed_version.release[0:2]),
                    [],
                ).append(host_compat_entry)

        host_repo_name = toolchain_info.name + "_host"
        if host_platforms:
            all_host_compatible_repo_names.append(host_repo_name)
            host_platforms = sorted_host_platforms(host_platforms)
            entries = host_platforms.values()
            host_compatible_python_repo(
                name = host_repo_name,
                base_name = host_repo_name,
                # NOTE: Order matters. The first found to be compatible is
                # (usually) used.
                platforms = host_platforms.keys(),
                os_names = {str(i): e.platform.os_name for i, e in enumerate(entries)},
                arch_names = {str(i): e.platform.arch for i, e in enumerate(entries)},
                python_versions = {str(i): e.full_python_version for i, e in enumerate(entries)},
                impl_repo_names = {str(i): e.impl_repo_name for i, e in enumerate(entries)},
            )
        else:
            needed_host_repos[host_repo_name] = struct(
                compatible_version = toolchain_info.python_version,
                full_python_version = full_python_version,
            )

    if needed_host_repos:
        for key, entries in all_host_compatible_impls.items():
            all_host_compatible_impls[key] = sorted(
                entries,
                reverse = True,
                key = lambda e: version.key(version.parse(e.full_python_version)),
            )

    for host_repo_name, info in needed_host_repos.items():
        choices = []
        if info.compatible_version not in all_host_compatible_impls:
            logger.warn("No host compatible runtime found compatible with version {}".format(info.compatible_version))
            continue

        choices = all_host_compatible_impls[info.compatible_version]
        platform_keys = [
            # We have to prepend the offset because the same platform
            # name might occur across different versions
            "{}_{}".format(i, entry.platform_name)
            for i, entry in enumerate(choices)
        ]
        platform_keys = sorted_host_platform_names(platform_keys)

        all_host_compatible_repo_names.append(host_repo_name)
        host_compatible_python_repo(
            name = host_repo_name,
            base_name = host_repo_name,
            platforms = platform_keys,
            impl_repo_names = {
                str(i): entry.impl_repo_name
                for i, entry in enumerate(choices)
            },
            os_names = {str(i): entry.platform.os_name for i, entry in enumerate(choices)},
            arch_names = {str(i): entry.platform.arch for i, entry in enumerate(choices)},
            python_versions = {str(i): entry.full_python_version for i, entry in enumerate(choices)},
        )

    # list[str] The infix to use for the resulting toolchain() `name` arg.
    toolchain_names = []

    # dict[str i, str repo]; where repo is the full repo name
    # ("python_3_10_unknown-linux-x86_64") for the toolchain
    # i corresponds to index `i` in toolchain_names
    toolchain_repo_names = {}

    # dict[str i, list[str] constraints]; where constraints is a list
    # of labels for target_compatible_with
    # i corresponds to index `i` in toolchain_names
    toolchain_tcw_map = {}

    # dict[str i, list[str] settings]; where settings is a list
    # of labels for target_settings
    # i corresponds to index `i` in toolchain_names
    toolchain_ts_map = {}

    # dict[str i, str set_constraint]; where set_constraint is the string
    # "True" or "False".
    # i corresponds to index `i` in toolchain_names
    toolchain_set_python_version_constraints = {}

    # dict[str i, str python_version]; where python_version is the full
    # python version ("3.4.5").
    toolchain_python_versions = {}

    # dict[str i, str platform_key]; where platform_key is the key within
    # the PLATFORMS global for this toolchain
    toolchain_platform_keys = {}

    # Split the toolchain info into separate objects so they can be passed onto
    # the repository rule.
    for entry in toolchain_impls:
        key = str(len(toolchain_names))

        toolchain_names.append(entry.name)
        toolchain_repo_names[key] = entry.impl_repo_name
        toolchain_tcw_map[key] = entry.platform.compatible_with

        # The target_settings attribute may not be present for users
        # patching python/versions.bzl.
        toolchain_ts_map[key] = getattr(entry.platform, "target_settings", [])
        toolchain_platform_keys[key] = entry.platform_name
        toolchain_python_versions[key] = entry.full_python_version

        # Repo rules can't accept dict[str, bool], so encode them as a string value.
        toolchain_set_python_version_constraints[key] = (
            "True" if entry.set_python_version_constraint else "False"
        )

    hub_repo(
        name = "pythons_hub",
        toolchain_names = toolchain_names,
        toolchain_repo_names = toolchain_repo_names,
        toolchain_target_compatible_with_map = toolchain_tcw_map,
        toolchain_target_settings_map = toolchain_ts_map,
        toolchain_platform_keys = toolchain_platform_keys,
        toolchain_python_versions = toolchain_python_versions,
        toolchain_set_python_version_constraints = toolchain_set_python_version_constraints,
        host_compatible_repo_names = sorted(all_host_compatible_repo_names),
        default_python_version = py.default_python_version,
        minor_mapping = py.config.minor_mapping,
        python_versions = list(py.config.default["tool_versions"].keys()),
    )

    # This is require in order to support multiple version py_test
    # and py_binary
    multi_toolchain_aliases(
        name = "python_versions",
        python_versions = {
            toolchain.python_version: toolchain.name
            for toolchain in py.toolchains
        },
    )

    if py.debug_info != None:
        _debug_repo(
            name = "rules_python_bzlmod_debug",
            debug_info = json.encode_indent(py.debug_info),
        )

    if bazel_features.external_deps.extension_metadata_has_reproducible:
        return module_ctx.extension_metadata(reproducible = True)
    else:
        return None

def _is_compatible_with_host(mctx, platform_info):
    os_name = repo_utils.get_platforms_os_name(mctx)
    cpu_name = repo_utils.get_platforms_cpu_name(mctx)
    return platform_info.os_name == os_name and platform_info.arch == cpu_name

def _one_or_the_same(first, second, *, onerror = None):
    if not first:
        return second
    if not second or second == first:
        return first
    if onerror:
        return onerror(first, second)
    else:
        fail("Unique value needed, got both '{}' and '{}', which are different".format(
            first,
            second,
        ))

def _fail_duplicate_module_toolchain_version(version, module):
    fail(("Duplicate module toolchain version: module '{module}' attempted " +
          "to use version '{version}' multiple times in itself").format(
        version = version,
        module = module,
    ))

def _warn_duplicate_global_toolchain_version(version, first, second_toolchain_name, second_module_name, logger):
    if not logger:
        return

    logger.info(lambda: (
        "Ignoring toolchain '{second_toolchain}' from module '{second_module}': " +
        "Toolchain '{first_toolchain}' from module '{first_module}' " +
        "already registered Python version {version} and has precedence."
    ).format(
        first_toolchain = first.name,
        first_module = first.module.name,
        second_module = second_module_name,
        second_toolchain = second_toolchain_name,
        version = version,
    ))

def _fail_multiple_defaults_python_version(first, second):
    fail(("Multiple python_version entries in defaults: " +
          "First default was python_version '{first}'. " +
          "Second was python_version '{second}'").format(
        first = first,
        second = second,
    ))

def _fail_multiple_defaults_python_version_file(first, second):
    fail(("Multiple python_version_file entries in defaults: " +
          "First default was python_version_file '{first}'. " +
          "Second was python_version_file '{second}'").format(
        first = first,
        second = second,
    ))

def _fail_multiple_defaults_python_version_env(first, second):
    fail(("Multiple python_version_env entries in defaults: " +
          "First default was python_version_env '{first}'. " +
          "Second was python_version_env '{second}'").format(
        first = first,
        second = second,
    ))

def _fail_multiple_default_toolchains(first, second):
    fail(("Multiple default toolchains: only one toolchain " +
          "can have is_default=True. First default " +
          "was toolchain '{first}'. Second was '{second}'").format(
        first = first,
        second = second,
    ))

def _validate_version(version_str, *, _fail = fail):
    v = version.parse(version_str, strict = True, _fail = _fail)
    if v == None:
        # Only reachable in tests
        return False

    if len(v.release) < 3:
        _fail("The 'python_version' attribute needs to specify the full version in at least 'X.Y.Z' format, got: '{}'".format(v.string))
        return False

    return True

def _process_single_version_overrides(*, tag, _fail = fail, default):
    if not _validate_version(tag.python_version, _fail = _fail):
        return

    available_versions = default["tool_versions"]
    kwargs = default.setdefault("kwargs", {})

    if tag.sha256 or tag.urls:
        if not (tag.sha256 and tag.urls):
            _fail("Both `sha256` and `urls` overrides need to be provided together")
            return

        for platform in (tag.sha256 or []):
            if platform not in default["platforms"]:
                _fail("The platform must be one of {allowed} but got '{got}'".format(
                    allowed = sorted(default["platforms"]),
                    got = platform,
                ))
                return

    sha256 = dict(tag.sha256) or available_versions[tag.python_version]["sha256"]
    override = {
        "sha256": sha256,
        "strip_prefix": {
            platform: tag.strip_prefix
            for platform in sha256
        },
        "url": {
            platform: list(tag.urls)
            for platform in tag.sha256
        } or available_versions[tag.python_version]["url"],
    }

    if tag.patches:
        override["patch_strip"] = {
            platform: tag.patch_strip
            for platform in sha256
        }
        override["patches"] = {
            platform: list(tag.patches)
            for platform in sha256
        }

    available_versions[tag.python_version] = {k: v for k, v in override.items() if v}

    if tag.distutils_content:
        kwargs.setdefault(tag.python_version, {})["distutils_content"] = tag.distutils_content
    if tag.distutils:
        kwargs.setdefault(tag.python_version, {})["distutils"] = tag.distutils

def _process_single_version_platform_overrides(*, tag, _fail = fail, default):
    if not _validate_version(tag.python_version, _fail = _fail):
        return

    available_versions = default["tool_versions"]

    if tag.python_version not in available_versions:
        if not tag.urls or not tag.sha256 or not tag.strip_prefix:
            _fail("When introducing a new python_version '{}', 'sha256', 'strip_prefix' and 'urls' must be specified".format(tag.python_version))
            return
        available_versions[tag.python_version] = {}

    if tag.coverage_tool:
        available_versions[tag.python_version].setdefault("coverage_tool", {})[tag.platform] = tag.coverage_tool
    if tag.patch_strip:
        available_versions[tag.python_version].setdefault("patch_strip", {})[tag.platform] = tag.patch_strip
    if tag.patches:
        available_versions[tag.python_version].setdefault("patches", {})[tag.platform] = list(tag.patches)
    if tag.sha256:
        available_versions[tag.python_version].setdefault("sha256", {})[tag.platform] = tag.sha256
    if tag.strip_prefix:
        available_versions[tag.python_version].setdefault("strip_prefix", {})[tag.platform] = tag.strip_prefix

    if tag.urls:
        available_versions[tag.python_version].setdefault("url", {})[tag.platform] = tag.urls

    # If platform is customized, or doesn't exist, (re)define one.
    if ((tag.target_compatible_with or tag.target_settings or tag.os_name or tag.arch) or
        tag.platform not in default["platforms"]):
        os_name = tag.os_name
        arch = tag.arch

        if not tag.target_compatible_with:
            target_compatible_with = []
            if os_name:
                target_compatible_with.append("@platforms//os:{}".format(
                    repo_utils.get_platforms_os_name(os_name),
                ))
            if arch:
                target_compatible_with.append("@platforms//cpu:{}".format(
                    repo_utils.get_platforms_cpu_name(arch),
                ))
        else:
            target_compatible_with = tag.target_compatible_with

        # For lack of a better option, give a bogus value. It only affects
        # if the runtime is considered host-compatible.
        if not os_name:
            os_name = "UNKNOWN_CUSTOM_OS"
        if not arch:
            arch = "UNKNOWN_CUSTOM_ARCH"

        # Move the override earlier in the ordering -- the platform key ordering
        # becomes the toolchain ordering within the version. This allows the
        # override to have a superset of constraints from a regular runtimes
        # (e.g. same platform, but with a custom flag required).
        override_first = {
            tag.platform: platform_info(
                compatible_with = target_compatible_with,
                target_settings = tag.target_settings,
                os_name = os_name,
                arch = arch,
            ),
        }
        for key, value in default["platforms"].items():
            # Don't replace our override with the old value
            if key in override_first:
                continue
            override_first[key] = value

        default["platforms"] = override_first

def _process_global_overrides(*, tag, default, _fail = fail):
    if tag.available_python_versions:
        available_versions = default["tool_versions"]
        all_versions = dict(available_versions)
        available_versions.clear()
        for v in tag.available_python_versions:
            if v not in all_versions:
                _fail("unknown version '{}', known versions are: {}".format(
                    v,
                    sorted(all_versions),
                ))
                return

            available_versions[v] = all_versions[v]

    if tag.minor_mapping:
        for minor_version, full_version in tag.minor_mapping.items():
            parsed = version.parse(minor_version, strict = True, _fail = _fail)
            if len(parsed.release) > 2 or parsed.pre or parsed.post or parsed.dev or parsed.local:
                fail("Expected the key to be of `X.Y` format but got `{}`".format(parsed.string))

            # Ensure that the version is valid
            version.parse(full_version, strict = True, _fail = _fail)

        default["minor_mapping"] = tag.minor_mapping

    forwarded_attrs = sorted(AUTH_ATTRS) + [
        "ignore_root_user_error",
        "base_url",
        "register_all_versions",
    ]
    for key in forwarded_attrs:
        if getattr(tag, key, None):
            default[key] = getattr(tag, key)

def _override_defaults(*overrides, modules, _fail = fail, default):
    mod = modules[0] if modules else None
    if not mod or not mod.is_root:
        return

    overriden_keys = []

    for override in overrides:
        for tag in getattr(mod.tags, override.name):
            key = override.key(tag)
            if key not in overriden_keys:
                overriden_keys.append(key)
            elif key:
                _fail("Only a single 'python.{}' can be present for '{}'".format(override.name, key))
                return
            else:
                _fail("Only a single 'python.{}' can be present".format(override.name))
                return

            override.fn(tag = tag, _fail = _fail, default = default)

def _get_toolchain_config(*, modules, _fail = fail):
    """Computes the configs for toolchains.

    Args:
        modules: The modules from module_ctx
        _fail: Function to call for failing; only used for testing.

    Returns:
        A struct with the following:
        * `kwargs`: {type}`dict[str, dict[str, object]` custom kwargs to pass to
          `python_register_toolchains`, keyed by python version.
          The first key is either a Major.Minor or Major.Minor.Patch
          string.
        * `minor_mapping`: {type}`dict[str, str]` the mapping of Major.Minor
          to Major.Minor.Patch.
        * `default`: {type}`dict[str, object]` of kwargs passed along to
          `python_register_toolchains`. These keys take final precedence.
        * `register_all_versions`: {type}`bool` whether all known versions
          should be registered.
    """

    # Items that can be overridden
    available_versions = {}
    for py_version, item in TOOL_VERSIONS.items():
        available_versions[py_version] = {}
        available_versions[py_version]["sha256"] = dict(item["sha256"])
        platforms = item["sha256"].keys()

        strip_prefix = item["strip_prefix"]
        if type(strip_prefix) == type(""):
            available_versions[py_version]["strip_prefix"] = {
                platform: strip_prefix
                for platform in platforms
            }
        else:
            available_versions[py_version]["strip_prefix"] = dict(strip_prefix)
        url = item["url"]
        if type(url) == type(""):
            available_versions[py_version]["url"] = {
                platform: url
                for platform in platforms
            }
        else:
            available_versions[py_version]["url"] = dict(url)

    default = {
        "base_url": DEFAULT_RELEASE_BASE_URL,
        "platforms": dict(PLATFORMS),  # Copy so it's mutable.
        "tool_versions": available_versions,
    }

    _override_defaults(
        # First override by single version, because the sha256 will replace
        # anything that has been there before.
        struct(
            name = "single_version_override",
            key = lambda t: t.python_version,
            fn = _process_single_version_overrides,
        ),
        # Then override particular platform entries if they need to be overridden.
        struct(
            name = "single_version_platform_override",
            key = lambda t: (t.python_version, t.platform),
            fn = _process_single_version_platform_overrides,
        ),
        # Then finally add global args and remove the unnecessary toolchains.
        # This ensures that we can do further validations when removing.
        struct(
            name = "override",
            key = lambda t: None,
            fn = _process_global_overrides,
        ),
        modules = modules,
        default = default,
        _fail = _fail,
    )

    register_all_versions = default.pop("register_all_versions", False)
    kwargs = default.pop("kwargs", {})

    versions = {}
    for version_string in available_versions:
        v = version.parse(version_string, strict = True)
        versions.setdefault(
            "{}.{}".format(v.release[0], v.release[1]),
            [],
        ).append((version.key(v), v.string))

    minor_mapping = {
        major_minor: max(subset)[1]
        for major_minor, subset in versions.items()
    }

    # The following ensures that all of the versions will be present in the minor_mapping
    minor_mapping_overrides = default.pop("minor_mapping", {})
    for major_minor, full in minor_mapping_overrides.items():
        minor_mapping[major_minor] = full

    return struct(
        kwargs = kwargs,
        minor_mapping = minor_mapping,
        default = default,
        register_all_versions = register_all_versions,
    )

def _create_defaults_attr_structs(*, mod):
    arg_structs = []

    for tag in mod.tags.defaults:
        arg_structs.append(_create_defaults_attr_struct(tag = tag))

    return arg_structs

def _create_defaults_attr_struct(*, tag):
    return struct(
        python_version = getattr(tag, "python_version", None),
        python_version_env = getattr(tag, "python_version_env", None),
        python_version_file = getattr(tag, "python_version_file", None),
    )

def _create_toolchain_attr_structs(*, mod, config, seen_versions):
    arg_structs = []

    for tag in mod.tags.toolchain:
        arg_structs.append(_create_toolchain_attrs_struct(
            tag = tag,
            toolchain_tag_count = len(mod.tags.toolchain),
        ))

        seen_versions[tag.python_version] = True

    if config.register_all_versions:
        arg_structs.extend([
            _create_toolchain_attrs_struct(python_version = v)
            for v in config.default["tool_versions"].keys() + config.minor_mapping.keys()
            if v not in seen_versions
        ])

    return arg_structs

def _create_toolchain_attrs_struct(*, tag = None, python_version = None, toolchain_tag_count = None):
    if tag and python_version:
        fail("Only one of tag and python version can be specified")
    if tag:
        # A single toolchain is treated as the default because it's unambiguous.
        is_default = tag.is_default or toolchain_tag_count == 1
    else:
        is_default = False

    return struct(
        is_default = is_default,
        python_version = python_version if python_version else tag.python_version,
        configure_coverage_tool = getattr(tag, "configure_coverage_tool", False),
        ignore_root_user_error = getattr(tag, "ignore_root_user_error", True),
    )

def _get_bazel_version_specific_kwargs():
    kwargs = {}

    if IS_BAZEL_6_4_OR_HIGHER:
        kwargs["environ"] = ["RULES_PYTHON_BZLMOD_DEBUG"]

    return kwargs

_defaults = tag_class(
    doc = """Tag class to specify the default Python version.""",
    attrs = {
        "python_version": attr.string(
            mandatory = False,
            doc = """\
String saying what the default Python version should be. If the string
matches the {attr}`python_version` attribute of a toolchain, this
toolchain is the default version. If this attribute is set, the
{attr}`is_default` attribute of the toolchain is ignored.

:::{versionadded} 1.4.0
:::
""",
        ),
        "python_version_env": attr.string(
            mandatory = False,
            doc = """\
Environment variable saying what the default Python version should be.
If the string matches the {attr}`python_version` attribute of a
toolchain, this toolchain is the default version. If this attribute is
set, the {attr}`is_default` attribute of the toolchain is ignored.

:::{versionadded} 1.4.0
:::
""",
        ),
        "python_version_file": attr.label(
            mandatory = False,
            allow_single_file = True,
            doc = """\
File saying what the default Python version should be. If the contents
of the file match the {attr}`python_version` attribute of a toolchain,
this toolchain is the default version. If this attribute is set, the
{attr}`is_default` attribute of the toolchain is ignored.

:::{versionadded} 1.4.0
:::
""",
        ),
    },
)

_toolchain = tag_class(
    doc = """Tag class used to register Python toolchains.
Use this tag class to register one or more Python toolchains. This class
is also potentially called by sub modules. The following covers different
business rules and use cases.

:::{topic} Toolchains in the Root Module

This class registers all toolchains in the root module.
:::

:::{topic} Toolchains in Sub Modules

It will create a toolchain that is in a sub module, if the toolchain
of the same name does not exist in the root module.  The extension stops name
clashing between toolchains in the root module and toolchains in sub modules.
You cannot configure more than one toolchain as the default toolchain.
:::

:::{topic} Toolchain set as the default version

This extension will not create a toolchain that exists in a sub module,
if the sub module toolchain is marked as the default version. If you have
more than one toolchain in your root module, you need to set one of the
toolchains as the default version.  If there is only one toolchain it
is set as the default toolchain.
:::

:::{topic} Toolchain repository name

A toolchain's repository name uses the format `python_{major}_{minor}`, e.g.
`python_3_10`. The `major` and `minor` components are
`major` and `minor` are the Python version from the `python_version` attribute.

If a toolchain is registered in `X.Y.Z`, then similarly the toolchain name will
be `python_{major}_{minor}_{patch}`, e.g. `python_3_10_19`.
:::

:::{topic} Toolchain detection
The definition of the first toolchain wins, which means that the root module
can override settings for any python toolchain available. This relies on the
documented module traversal from the {obj}`module_ctx.modules`.
:::

:::{tip}
In order to use a different name than the above, you can use the following `MODULE.bazel`
syntax:
```starlark
python = use_extension("@rules_python//python/extensions:python.bzl", "python")
python.defaults(python_version = "3.11")
python.toolchain(python_version = "3.11")

use_repo(python, my_python_name = "python_3_11")
```

Then the python interpreter will be available as `my_python_name`.
:::
""",
    attrs = {
        "configure_coverage_tool": attr.bool(
            mandatory = False,
            doc = "Whether or not to configure the default coverage tool provided by `rules_python` for the compatible toolchains.",
        ),
        "ignore_root_user_error": attr.bool(
            default = True,
            doc = """\
The Python runtime installation is made read only. This improves the ability for
Bazel to cache it by preventing the interpreter from creating `.pyc` files for
the standard library dynamically at runtime as they are loaded (this often leads
to spurious cache misses or build failures).

However, if the user is running Bazel as root, this read-onlyness is not
respected. Bazel will print a warning message when it detects that the runtime
installation is writable despite being made read only (i.e. it's running with
root access) while this attribute is set `False`, however this messaging can be ignored by setting
this to `False`.
""",
            mandatory = False,
        ),
        "is_default": attr.bool(
            mandatory = False,
            doc = """\
Whether the toolchain is the default version.

:::{versionchanged} 1.4.0
This setting is ignored if the default version is set using the `defaults`
tag class (encouraged).
:::
""",
        ),
        "python_version": attr.string(
            mandatory = True,
            doc = """\
The Python version, in `major.minor` or `major.minor.patch` format, e.g
`3.12` (or `3.12.3`), to create a toolchain for.
""",
        ),
    },
)

_override = tag_class(
    doc = """Tag class used to override defaults and behaviour of the module extension.

:::{versionadded} 0.36.0
:::
""",
    attrs = {
        "available_python_versions": attr.string_list(
            mandatory = False,
            doc = """\
The list of available python tool versions to use. Must be in `X.Y.Z` format.
If the unknown version given the processing of the extension will fail - all of
the versions in the list have to be defined with
{obj}`python.single_version_override` or
{obj}`python.single_version_platform_override` before they are used in this
list.

This attribute is usually used in order to ensure that no unexpected transitive
dependencies are introduced.
""",
        ),
        "base_url": attr.string(
            mandatory = False,
            doc = "The base URL to be used when downloading toolchains.",
            default = DEFAULT_RELEASE_BASE_URL,
        ),
        "ignore_root_user_error": attr.bool(
            default = True,
            doc = """Deprecated; do not use. This attribute has no effect.""",
            mandatory = False,
        ),
        "minor_mapping": attr.string_dict(
            mandatory = False,
            doc = """\
The mapping between `X.Y` to `X.Y.Z` versions to be used when setting up
toolchains. It defaults to the interpreter with the highest available patch
version for each minor version. For example if one registers `3.10.3`, `3.10.4`
and `3.11.4` then the default for the `minor_mapping` dict will be:
```starlark
{
"3.10": "3.10.4",
"3.11": "3.11.4",
}
```

:::{versionchanged} 0.37.0
The values in this mapping override the default values and do not replace them.
:::
""",
            default = {},
        ),
        "register_all_versions": attr.bool(default = False, doc = "Add all versions"),
    } | AUTH_ATTRS,
)

_single_version_override = tag_class(
    doc = """Override single python version URLs and patches for all platforms.

:::{note}
This will replace any existing configuration for the given python version.
:::

:::{tip}
If you would like to modify the configuration for a specific `(version,
platform)`, please use the {obj}`single_version_platform_override` tag
class.
:::

:::{versionadded} 0.36.0
:::
""",
    attrs = {
        # NOTE @aignas 2024-09-01: all of the attributes except for `version`
        # can be part of the `python.toolchain` call. That would make it more
        # ergonomic to define new toolchains and to override values for old
        # toolchains. The same semantics of the `first one wins` would apply,
        # so technically there is no need for any overrides?
        #
        # Although these attributes would override the code that is used by the
        # code in non-root modules, so technically this could be thought as
        # being overridden.
        #
        # rules_go has a single download call:
        # https://github.com/bazelbuild/rules_go/blob/master/go/private/extensions.bzl#L38
        #
        # However, we need to understand how to accommodate the fact that
        # {attr}`single_version_override.version` only allows patch versions.
        "distutils": attr.label(
            allow_single_file = True,
            doc = "A distutils.cfg file to be included in the Python installation. " +
                  "Either {attr}`distutils` or {attr}`distutils_content` can be specified, but not both.",
            mandatory = False,
        ),
        "distutils_content": attr.string(
            doc = "A distutils.cfg file content to be included in the Python installation. " +
                  "Either {attr}`distutils` or {attr}`distutils_content` can be specified, but not both.",
            mandatory = False,
        ),
        "patch_strip": attr.int(
            mandatory = False,
            doc = "Same as the --strip argument of Unix patch.",
            default = 0,
        ),
        "patches": attr.label_list(
            mandatory = False,
            doc = "A list of labels pointing to patch files to apply for the interpreter repository. They are applied in the list order and are applied before any platform-specific patches are applied.",
        ),
        "python_version": attr.string(
            mandatory = True,
            doc = "The python version to override URLs for. Must be in `X.Y.Z` format.",
        ),
        "sha256": attr.string_dict(
            mandatory = False,
            doc = "The python platform to sha256 dict. See {attr}`python.single_version_platform_override.platform` for allowed key values.",
        ),
        "strip_prefix": attr.string(
            mandatory = False,
            doc = "The 'strip_prefix' for the archive, defaults to 'python'.",
            default = "python",
        ),
        "urls": attr.string_list(
            mandatory = False,
            doc = "The URL template to fetch releases for this Python version. See {attr}`python.single_version_platform_override.urls` for documentation.",
        ),
    },
)

_single_version_platform_override = tag_class(
    doc = """Override single python version for a single existing platform.

If the `(version, platform)` is new, we will add it to the existing versions and will
use the same `url` template.

:::{tip}
If you would like to add or remove platforms to a single python version toolchain
configuration, please use {obj}`single_version_override`.
:::

:::{versionadded} 0.36.0
:::
""",
    attrs = {
        "arch": attr.string(
            doc = """
The arch (cpu) the runtime is compatible with.

If not set, then the runtime cannot be used as a `python_X_Y_host` runtime.

If set, the `os_name`, `target_compatible_with` and `target_settings` attributes
should also be set.

The values should be one of the values in `@platforms//cpu`

:::{seealso}
Docs for [Registering custom runtimes]
:::

:::{{versionadded}} 1.5.0
:::
""",
        ),
        "coverage_tool": attr.label(
            doc = """\
The coverage tool to be used for a particular Python interpreter. This can override
`rules_python` defaults.
""",
        ),
        "os_name": attr.string(
            doc = """
The host OS the runtime is compatible with.

If not set, then the runtime cannot be used as a `python_X_Y_host` runtime.

If set, the `os_name`, `target_compatible_with` and `target_settings` attributes
should also be set.

The values should be one of the values in `@platforms//os`

:::{seealso}
Docs for [Registering custom runtimes]
:::

:::{{versionadded}} 1.5.0
:::
""",
        ),
        "patch_strip": attr.int(
            mandatory = False,
            doc = "Same as the --strip argument of Unix patch.",
            default = 0,
        ),
        "patches": attr.label_list(
            mandatory = False,
            doc = "A list of labels pointing to patch files to apply for the interpreter repository. They are applied in the list order and are applied after the common patches are applied.",
        ),
        "platform": attr.string(
            mandatory = True,
            doc = """
The platform to override the values for, typically one of:\n
{platforms}

Other values are allowed, in which case, `target_compatible_with`,
`target_settings`, `os_name`, and `arch` should be specified so the toolchain is
only used when appropriate.

:::{{versionchanged}} 1.5.0
Arbitrary platform strings allowed.
:::
""".format(
                platforms = "\n".join(sorted(["* `{}`".format(p) for p in PLATFORMS])),
            ),
        ),
        "python_version": attr.string(
            mandatory = True,
            doc = "The python version to override URLs for. Must be in `X.Y.Z` format.",
        ),
        "sha256": attr.string(
            mandatory = False,
            doc = "The sha256 for the archive",
        ),
        "strip_prefix": attr.string(
            mandatory = False,
            doc = "The 'strip_prefix' for the archive, defaults to 'python'.",
            default = "python",
        ),
        "target_compatible_with": attr.string_list(
            doc = """
The `target_compatible_with` values to use for the toolchain definition.

If not set, then `os_name` and `arch` will be used to populate it.

If set, `target_settings`, `os_name`, and `arch` should also be set.

:::{seealso}
Docs for [Registering custom runtimes]
:::

:::{{versionadded}} 1.5.0
:::
""",
        ),
        "target_settings": attr.string_list(
            doc = """
The `target_setings` values to use for the toolchain definition.

If set, `target_compatible_with`, `os_name`, and `arch` should also be set.

:::{seealso}
Docs for [Registering custom runtimes]
:::

:::{{versionadded}} 1.5.0
:::
""",
        ),
        "urls": attr.string_list(
            mandatory = False,
            doc = "The URL template to fetch releases for this Python version. If the URL template results in a relative fragment, default base URL is going to be used. Occurrences of `{python_version}`, `{platform}` and `{build}` will be interpolated based on the contents in the override and the known {attr}`platform` values.",
        ),
    },
)

python = module_extension(
    doc = """Bzlmod extension that is used to register Python toolchains.
""",
    implementation = _python_impl,
    tag_classes = {
        "defaults": _defaults,
        "override": _override,
        "single_version_override": _single_version_override,
        "single_version_platform_override": _single_version_platform_override,
        "toolchain": _toolchain,
    },
    **_get_bazel_version_specific_kwargs()
)

_DEBUG_BUILD_CONTENT = """
package(
    default_visibility = ["//visibility:public"],
)
exports_files(["debug_info.json"])
"""

def _debug_repo_impl(repo_ctx):
    repo_ctx.file("BUILD.bazel", _DEBUG_BUILD_CONTENT)
    repo_ctx.file("debug_info.json", repo_ctx.attr.debug_info)

_debug_repo = repository_rule(
    implementation = _debug_repo_impl,
    attrs = {
        "debug_info": attr.string(),
    },
)
