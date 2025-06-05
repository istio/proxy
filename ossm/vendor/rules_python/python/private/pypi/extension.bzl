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

"pip module extension for use with bzlmod"

load("@bazel_features//:features.bzl", "bazel_features")
load("@pythons_hub//:interpreters.bzl", "INTERPRETER_LABELS")
load("//python/private:auth.bzl", "AUTH_ATTRS")
load("//python/private:normalize_name.bzl", "normalize_name")
load("//python/private:repo_utils.bzl", "repo_utils")
load("//python/private:semver.bzl", "semver")
load("//python/private:version_label.bzl", "version_label")
load(":attrs.bzl", "use_isolated")
load(":evaluate_markers.bzl", "evaluate_markers", EVALUATE_MARKERS_SRCS = "SRCS")
load(":hub_repository.bzl", "hub_repository", "whl_config_settings_to_json")
load(":parse_requirements.bzl", "parse_requirements")
load(":parse_whl_name.bzl", "parse_whl_name")
load(":pip_repository_attrs.bzl", "ATTRS")
load(":requirements_files_by_platform.bzl", "requirements_files_by_platform")
load(":simpleapi_download.bzl", "simpleapi_download")
load(":whl_config_setting.bzl", "whl_config_setting")
load(":whl_library.bzl", "whl_library")
load(":whl_repo_name.bzl", "pypi_repo_name", "whl_repo_name")

def _major_minor_version(version):
    version = semver(version)
    return "{}.{}".format(version.major, version.minor)

def _whl_mods_impl(whl_mods_dict):
    """Implementation of the pip.whl_mods tag class.

    This creates the JSON files used to modify the creation of different wheels.
"""
    for hub_name, whl_maps in whl_mods_dict.items():
        whl_mods = {}

        # create a struct that we can pass to the _whl_mods_repo rule
        # to create the different JSON files.
        for whl_name, mods in whl_maps.items():
            whl_mods[whl_name] = json.encode(struct(
                additive_build_content = mods.build_content,
                copy_files = mods.copy_files,
                copy_executables = mods.copy_executables,
                data = mods.data,
                data_exclude_glob = mods.data_exclude_glob,
                srcs_exclude_glob = mods.srcs_exclude_glob,
            ))

        _whl_mods_repo(
            name = hub_name,
            whl_mods = whl_mods,
        )

def _create_whl_repos(
        module_ctx,
        *,
        pip_attr,
        whl_overrides,
        evaluate_markers = evaluate_markers,
        available_interpreters = INTERPRETER_LABELS,
        get_index_urls = None):
    """create all of the whl repositories

    Args:
        module_ctx: {type}`module_ctx`.
        pip_attr: {type}`struct` - the struct that comes from the tag class iteration.
        whl_overrides: {type}`dict[str, struct]` - per-wheel overrides.
        evaluate_markers: the function to use to evaluate markers.
        get_index_urls: A function used to get the index URLs
        available_interpreters: {type}`dict[str, Label]` The dictionary of available
            interpreters that have been registered using the `python` bzlmod extension.
            The keys are in the form `python_{snake_case_version}_host`. This is to be
            used during the `repository_rule` and must be always compatible with the host.

    Returns a {type}`struct` with the following attributes:
        whl_map: {type}`dict[str, list[struct]]` the output is keyed by the
            normalized package name and the values are the instances of the
            {bzl:obj}`whl_config_setting` return values.
        exposed_packages: {type}`dict[str, Any]` this is just a way to
            represent a set of string values.
        whl_libraries: {type}`dict[str, dict[str, Any]]` the keys are the
            aparent repository names for the hub repo and the values are the
            arguments that will be passed to {bzl:obj}`whl_library` repository
            rule.
    """
    logger = repo_utils.logger(module_ctx, "pypi:create_whl_repos")
    python_interpreter_target = pip_attr.python_interpreter_target

    # containers to aggregate outputs from this function
    whl_map = {}
    extra_aliases = {
        whl_name: {alias: True for alias in aliases}
        for whl_name, aliases in pip_attr.extra_hub_aliases.items()
    }
    whl_libraries = {}

    # if we do not have the python_interpreter set in the attributes
    # we programmatically find it.
    hub_name = pip_attr.hub_name
    if python_interpreter_target == None and not pip_attr.python_interpreter:
        python_name = "python_{}_host".format(
            pip_attr.python_version.replace(".", "_"),
        )
        if python_name not in available_interpreters:
            fail((
                "Unable to find interpreter for pip hub '{hub_name}' for " +
                "python_version={version}: Make sure a corresponding " +
                '`python.toolchain(python_version="{version}")` call exists.' +
                "Expected to find {python_name} among registered versions:\n  {labels}"
            ).format(
                hub_name = hub_name,
                version = pip_attr.python_version,
                python_name = python_name,
                labels = "  \n".join(available_interpreters),
            ))
        python_interpreter_target = available_interpreters[python_name]

    pip_name = "{}_{}".format(
        hub_name,
        version_label(pip_attr.python_version),
    )
    major_minor = _major_minor_version(pip_attr.python_version)

    whl_modifications = {}
    if pip_attr.whl_modifications != None:
        for mod, whl_name in pip_attr.whl_modifications.items():
            whl_modifications[normalize_name(whl_name)] = mod

    if pip_attr.experimental_requirement_cycles:
        requirement_cycles = {
            name: [normalize_name(whl_name) for whl_name in whls]
            for name, whls in pip_attr.experimental_requirement_cycles.items()
        }

        whl_group_mapping = {
            whl_name: group_name
            for group_name, group_whls in requirement_cycles.items()
            for whl_name in group_whls
        }
    else:
        whl_group_mapping = {}
        requirement_cycles = {}

    requirements_by_platform = parse_requirements(
        module_ctx,
        requirements_by_platform = requirements_files_by_platform(
            requirements_by_platform = pip_attr.requirements_by_platform,
            requirements_linux = pip_attr.requirements_linux,
            requirements_lock = pip_attr.requirements_lock,
            requirements_osx = pip_attr.requirements_darwin,
            requirements_windows = pip_attr.requirements_windows,
            extra_pip_args = pip_attr.extra_pip_args,
            python_version = major_minor,
            logger = logger,
        ),
        extra_pip_args = pip_attr.extra_pip_args,
        get_index_urls = get_index_urls,
        # NOTE @aignas 2024-08-02: , we will execute any interpreter that we find either
        # in the PATH or if specified as a label. We will configure the env
        # markers when evaluating the requirement lines based on the output
        # from the `requirements_files_by_platform` which should have something
        # similar to:
        # {
        #    "//:requirements.txt": ["cp311_linux_x86_64", ...]
        # }
        #
        # We know the target python versions that we need to evaluate the
        # markers for and thus we don't need to use multiple python interpreter
        # instances to perform this manipulation. This function should be executed
        # only once by the underlying code to minimize the overhead needed to
        # spin up a Python interpreter.
        evaluate_markers = lambda module_ctx, requirements: evaluate_markers(
            module_ctx,
            requirements = requirements,
            python_interpreter = pip_attr.python_interpreter,
            python_interpreter_target = python_interpreter_target,
            srcs = pip_attr._evaluate_markers_srcs,
            logger = logger,
        ),
        logger = logger,
    )

    for whl_name, requirements in requirements_by_platform.items():
        group_name = whl_group_mapping.get(whl_name)
        group_deps = requirement_cycles.get(group_name, [])

        # Construct args separately so that the lock file can be smaller and does not include unused
        # attrs.
        whl_library_args = dict(
            repo = pip_name,
            dep_template = "@{}//{{name}}:{{target}}".format(hub_name),
        )
        maybe_args = dict(
            # The following values are safe to omit if they have false like values
            annotation = whl_modifications.get(whl_name),
            download_only = pip_attr.download_only,
            enable_implicit_namespace_pkgs = pip_attr.enable_implicit_namespace_pkgs,
            environment = pip_attr.environment,
            envsubst = pip_attr.envsubst,
            experimental_target_platforms = pip_attr.experimental_target_platforms,
            group_deps = group_deps,
            group_name = group_name,
            pip_data_exclude = pip_attr.pip_data_exclude,
            python_interpreter = pip_attr.python_interpreter,
            python_interpreter_target = python_interpreter_target,
            whl_patches = {
                p: json.encode(args)
                for p, args in whl_overrides.get(whl_name, {}).items()
            },
        )
        whl_library_args.update({k: v for k, v in maybe_args.items() if v})
        maybe_args_with_default = dict(
            # The following values have defaults next to them
            isolated = (use_isolated(module_ctx, pip_attr), True),
            quiet = (pip_attr.quiet, True),
            timeout = (pip_attr.timeout, 600),
        )
        whl_library_args.update({
            k: v
            for k, (v, default) in maybe_args_with_default.items()
            if v != default
        })

        for requirement in requirements:
            for repo_name, (args, config_setting) in _whl_repos(
                requirement = requirement,
                whl_library_args = whl_library_args,
                download_only = pip_attr.download_only,
                netrc = pip_attr.netrc,
                auth_patterns = pip_attr.auth_patterns,
                python_version = major_minor,
                multiple_requirements_for_whl = len(requirements) > 1.,
            ).items():
                repo_name = "{}_{}".format(pip_name, repo_name)
                if repo_name in whl_libraries:
                    fail("Attempting to creating a duplicate library {} for {}".format(
                        repo_name,
                        whl_name,
                    ))

                whl_libraries[repo_name] = args
                whl_map.setdefault(whl_name, {})[config_setting] = repo_name

    return struct(
        whl_map = whl_map,
        exposed_packages = {
            whl_name: None
            for whl_name, requirements in requirements_by_platform.items()
            if len([r for r in requirements if r.is_exposed]) > 0
        },
        extra_aliases = extra_aliases,
        whl_libraries = whl_libraries,
    )

def _whl_repos(*, requirement, whl_library_args, download_only, netrc, auth_patterns, multiple_requirements_for_whl = False, python_version):
    ret = {}

    dists = requirement.whls
    if not download_only and requirement.sdist:
        dists = dists + [requirement.sdist]

    for distribution in dists:
        args = dict(whl_library_args)
        if netrc:
            args["netrc"] = netrc
        if auth_patterns:
            args["auth_patterns"] = auth_patterns

        if not distribution.filename.endswith(".whl"):
            # pip is not used to download wheels and the python
            # `whl_library` helpers are only extracting things, however
            # for sdists, they will be built by `pip`, so we still
            # need to pass the extra args there.
            args["extra_pip_args"] = requirement.extra_pip_args

        # This is no-op because pip is not used to download the wheel.
        args.pop("download_only", None)

        args["requirement"] = requirement.srcs.requirement
        args["urls"] = [distribution.url]
        args["sha256"] = distribution.sha256
        args["filename"] = distribution.filename
        args["experimental_target_platforms"] = requirement.target_platforms

        # Pure python wheels or sdists may need to have a platform here
        target_platforms = None
        if distribution.filename.endswith("-any.whl") or not distribution.filename.endswith(".whl"):
            if multiple_requirements_for_whl:
                target_platforms = requirement.target_platforms

        repo_name = whl_repo_name(
            distribution.filename,
            distribution.sha256,
        )
        ret[repo_name] = (
            args,
            whl_config_setting(
                version = python_version,
                filename = distribution.filename,
                target_platforms = target_platforms,
            ),
        )

    if ret:
        return ret

    # Fallback to a pip-installed wheel
    args = dict(whl_library_args)  # make a copy
    args["requirement"] = requirement.srcs.requirement_line
    if requirement.extra_pip_args:
        args["extra_pip_args"] = requirement.extra_pip_args

    if download_only:
        args.setdefault("experimental_target_platforms", requirement.target_platforms)

    target_platforms = requirement.target_platforms if multiple_requirements_for_whl else []
    repo_name = pypi_repo_name(
        normalize_name(requirement.distribution),
        *target_platforms
    )
    ret[repo_name] = (
        args,
        whl_config_setting(
            version = python_version,
            target_platforms = target_platforms or None,
        ),
    )

    return ret

def parse_modules(module_ctx, _fail = fail, simpleapi_download = simpleapi_download, **kwargs):
    """Implementation of parsing the tag classes for the extension and return a struct for registering repositories.

    Args:
        module_ctx: {type}`module_ctx` module context.
        simpleapi_download: Used for testing overrides
        _fail: {type}`function` the failure function, mainly for testing.
        **kwargs: Extra arguments passed to the layers below.

    Returns:
        A struct with the following attributes:
    """
    whl_mods = {}
    for mod in module_ctx.modules:
        for whl_mod in mod.tags.whl_mods:
            if whl_mod.whl_name in whl_mods.get(whl_mod.hub_name, {}):
                # We cannot have the same wheel name in the same hub, as we
                # will create the same JSON file name.
                _fail("""\
Found same whl_name '{}' in the same hub '{}', please use a different hub_name.""".format(
                    whl_mod.whl_name,
                    whl_mod.hub_name,
                ))
                return None

            build_content = whl_mod.additive_build_content
            if whl_mod.additive_build_content_file != None and whl_mod.additive_build_content != "":
                _fail("""\
You cannot use both the additive_build_content and additive_build_content_file arguments at the same time.
""")
                return None
            elif whl_mod.additive_build_content_file != None:
                build_content = module_ctx.read(whl_mod.additive_build_content_file)

            whl_mods.setdefault(whl_mod.hub_name, {})[whl_mod.whl_name] = struct(
                build_content = build_content,
                copy_files = whl_mod.copy_files,
                copy_executables = whl_mod.copy_executables,
                data = whl_mod.data,
                data_exclude_glob = whl_mod.data_exclude_glob,
                srcs_exclude_glob = whl_mod.srcs_exclude_glob,
            )

    _overriden_whl_set = {}
    whl_overrides = {}
    for module in module_ctx.modules:
        for attr in module.tags.override:
            if not module.is_root:
                # Overrides are only supported in root modules. Silently
                # ignore the override:
                continue

            if not attr.file.endswith(".whl"):
                fail("Only whl overrides are supported at this time")

            whl_name = normalize_name(parse_whl_name(attr.file).distribution)

            if attr.file in _overriden_whl_set:
                fail("Duplicate module overrides for '{}'".format(attr.file))
            _overriden_whl_set[attr.file] = None

            for patch in attr.patches:
                if whl_name not in whl_overrides:
                    whl_overrides[whl_name] = {}

                if patch not in whl_overrides[whl_name]:
                    whl_overrides[whl_name][patch] = struct(
                        patch_strip = attr.patch_strip,
                        whls = [],
                    )

                whl_overrides[whl_name][patch].whls.append(attr.file)

    # Used to track all the different pip hubs and the spoke pip Python
    # versions.
    pip_hub_map = {}
    simpleapi_cache = {}

    # Keeps track of all the hub's whl repos across the different versions.
    # dict[hub, dict[whl, dict[version, str pip]]]
    # Where hub, whl, and pip are the repo names
    hub_whl_map = {}
    hub_group_map = {}
    exposed_packages = {}
    extra_aliases = {}
    whl_libraries = {}

    is_reproducible = True

    for mod in module_ctx.modules:
        for pip_attr in mod.tags.parse:
            hub_name = pip_attr.hub_name
            if hub_name not in pip_hub_map:
                pip_hub_map[pip_attr.hub_name] = struct(
                    module_name = mod.name,
                    python_versions = [pip_attr.python_version],
                )
            elif pip_hub_map[hub_name].module_name != mod.name:
                # We cannot have two hubs with the same name in different
                # modules.
                fail((
                    "Duplicate cross-module pip hub named '{hub}': pip hub " +
                    "names must be unique across modules. First defined " +
                    "by module '{first_module}', second attempted by " +
                    "module '{second_module}'"
                ).format(
                    hub = hub_name,
                    first_module = pip_hub_map[hub_name].module_name,
                    second_module = mod.name,
                ))

            elif pip_attr.python_version in pip_hub_map[hub_name].python_versions:
                fail((
                    "Duplicate pip python version '{version}' for hub " +
                    "'{hub}' in module '{module}': the Python versions " +
                    "used for a hub must be unique"
                ).format(
                    hub = hub_name,
                    module = mod.name,
                    version = pip_attr.python_version,
                ))
            else:
                pip_hub_map[pip_attr.hub_name].python_versions.append(pip_attr.python_version)

            get_index_urls = None
            if pip_attr.experimental_index_url:
                is_reproducible = False
                get_index_urls = lambda ctx, distributions: simpleapi_download(
                    ctx,
                    attr = struct(
                        index_url = pip_attr.experimental_index_url,
                        extra_index_urls = pip_attr.experimental_extra_index_urls or [],
                        index_url_overrides = pip_attr.experimental_index_url_overrides or {},
                        sources = distributions,
                        envsubst = pip_attr.envsubst,
                        # Auth related info
                        netrc = pip_attr.netrc,
                        auth_patterns = pip_attr.auth_patterns,
                    ),
                    cache = simpleapi_cache,
                    parallel_download = pip_attr.parallel_download,
                )

            out = _create_whl_repos(
                module_ctx,
                pip_attr = pip_attr,
                get_index_urls = get_index_urls,
                whl_overrides = whl_overrides,
                **kwargs
            )
            hub_whl_map.setdefault(hub_name, {})
            for key, settings in out.whl_map.items():
                for setting, repo in settings.items():
                    hub_whl_map[hub_name].setdefault(key, {}).setdefault(repo, []).append(setting)
            extra_aliases.setdefault(hub_name, {})
            for whl_name, aliases in out.extra_aliases.items():
                extra_aliases[hub_name].setdefault(whl_name, {}).update(aliases)
            exposed_packages.setdefault(hub_name, {}).update(out.exposed_packages)
            whl_libraries.update(out.whl_libraries)

            # TODO @aignas 2024-04-05: how do we support different requirement
            # cycles for different abis/oses? For now we will need the users to
            # assume the same groups across all versions/platforms until we start
            # using an alternative cycle resolution strategy.
            hub_group_map[hub_name] = pip_attr.experimental_requirement_cycles

    return struct(
        # We sort so that the lock-file remains the same no matter the order of how the
        # args are manipulated in the code going before.
        whl_mods = dict(sorted(whl_mods.items())),
        hub_whl_map = {
            hub_name: {
                whl_name: dict(settings)
                for whl_name, settings in sorted(whl_map.items())
            }
            for hub_name, whl_map in sorted(hub_whl_map.items())
        },
        hub_group_map = {
            hub_name: {
                key: sorted(values)
                for key, values in sorted(group_map.items())
            }
            for hub_name, group_map in sorted(hub_group_map.items())
        },
        exposed_packages = {
            k: sorted(v)
            for k, v in sorted(exposed_packages.items())
        },
        extra_aliases = {
            hub_name: {
                whl_name: sorted(aliases)
                for whl_name, aliases in extra_whl_aliases.items()
            }
            for hub_name, extra_whl_aliases in extra_aliases.items()
        },
        whl_libraries = {
            k: dict(sorted(args.items()))
            for k, args in sorted(whl_libraries.items())
        },
        is_reproducible = is_reproducible,
    )

def _pip_impl(module_ctx):
    """Implementation of a class tag that creates the pip hub and corresponding pip spoke whl repositories.

    This implementation iterates through all of the `pip.parse` calls and creates
    different pip hub repositories based on the "hub_name".  Each of the
    pip calls create spoke repos that uses a specific Python interpreter.

    In a MODULES.bazel file we have:

    pip.parse(
        hub_name = "pip",
        python_version = 3.9,
        requirements_lock = "//:requirements_lock_3_9.txt",
        requirements_windows = "//:requirements_windows_3_9.txt",
    )
    pip.parse(
        hub_name = "pip",
        python_version = 3.10,
        requirements_lock = "//:requirements_lock_3_10.txt",
        requirements_windows = "//:requirements_windows_3_10.txt",
    )

    For instance, we have a hub with the name of "pip".
    A repository named the following is created. It is actually called last when
    all of the pip spokes are collected.

    - @@rules_python~override~pip~pip

    As shown in the example code above we have the following.
    Two different pip.parse statements exist in MODULE.bazel provide the hub_name "pip".
    These definitions create two different pip spoke repositories that are
    related to the hub "pip".
    One spoke uses Python 3.9 and the other uses Python 3.10. This code automatically
    determines the Python version and the interpreter.
    Both of these pip spokes contain requirements files that includes websocket
    and its dependencies.

    We also need repositories for the wheels that the different pip spokes contain.
    For each Python version a different wheel repository is created. In our example
    each pip spoke had a requirements file that contained websockets. We
    then create two different wheel repositories that are named the following.

    - @@rules_python~override~pip~pip_39_websockets
    - @@rules_python~override~pip~pip_310_websockets

    And if the wheel has any other dependencies subsequent wheels are created in the same fashion.

    The hub repository has aliases for `pkg`, `data`, etc, which have a select that resolves to
    a spoke repository depending on the Python version.

    Also we may have more than one hub as defined in a MODULES.bazel file.  So we could have multiple
    hubs pointing to various different pip spokes.

    Some other business rules notes. A hub can only have one spoke per Python version.  We cannot
    have a hub named "pip" that has two spokes that use the Python 3.9 interpreter.  Second
    we cannot have the same hub name used in sub-modules.  The hub name has to be globally
    unique.

    This implementation also handles the creation of whl_modification JSON files that are used
    during the creation of wheel libraries. These JSON files used via the annotations argument
    when calling wheel_installer.py.

    Args:
        module_ctx: module contents
    """

    mods = parse_modules(module_ctx)

    # Build all of the wheel modifications if the tag class is called.
    _whl_mods_impl(mods.whl_mods)

    for name, args in mods.whl_libraries.items():
        whl_library(name = name, **args)

    for hub_name, whl_map in mods.hub_whl_map.items():
        hub_repository(
            name = hub_name,
            repo_name = hub_name,
            extra_hub_aliases = mods.extra_aliases.get(hub_name, {}),
            whl_map = {
                key: whl_config_settings_to_json(values)
                for key, values in whl_map.items()
            },
            packages = mods.exposed_packages.get(hub_name, []),
            groups = mods.hub_group_map.get(hub_name),
        )

    if bazel_features.external_deps.extension_metadata_has_reproducible:
        # If we are not using the `experimental_index_url feature, the extension is fully
        # deterministic and we don't need to create a lock entry for it.
        #
        # In order to be able to dogfood the `experimental_index_url` feature before it gets
        # stabilized, we have created the `_pip_non_reproducible` function, that will result
        # in extra entries in the lock file.
        return module_ctx.extension_metadata(reproducible = mods.is_reproducible)
    else:
        return None

def _pip_parse_ext_attrs(**kwargs):
    """Get the attributes for the pip extension.

    Args:
        **kwargs: A kwarg for setting defaults for the specific attributes. The
        key is expected to be the same as the attribute key.

    Returns:
        A dict of attributes.
    """
    attrs = dict({
        "experimental_extra_index_urls": attr.string_list(
            doc = """\
The extra index URLs to use for downloading wheels using bazel downloader.
Each value is going to be subject to `envsubst` substitutions if necessary.

The indexes must support Simple API as described here:
https://packaging.python.org/en/latest/specifications/simple-repository-api/

This is equivalent to `--extra-index-urls` `pip` option.

:::{versionchanged} 1.1.0
Starting with this version we will iterate over each index specified until
we find metadata for all references distributions.
:::
""",
            default = [],
        ),
        "experimental_index_url": attr.string(
            default = kwargs.get("experimental_index_url", ""),
            doc = """\
The index URL to use for downloading wheels using bazel downloader. This value is going
to be subject to `envsubst` substitutions if necessary.

The indexes must support Simple API as described here:
https://packaging.python.org/en/latest/specifications/simple-repository-api/

In the future this could be defaulted to `https://pypi.org` when this feature becomes
stable.

This is equivalent to `--index-url` `pip` option.

:::{versionchanged} 0.37.0
If {attr}`download_only` is set, then `sdist` archives will be discarded and `pip.parse` will
operate in wheel-only mode.
:::
""",
        ),
        "experimental_index_url_overrides": attr.string_dict(
            doc = """\
The index URL overrides for each package to use for downloading wheels using
bazel downloader. This value is going to be subject to `envsubst` substitutions
if necessary.

The key is the package name (will be normalized before usage) and the value is the
index URL.

This design pattern has been chosen in order to be fully deterministic about which
packages come from which source. We want to avoid issues similar to what happened in
https://pytorch.org/blog/compromised-nightly-dependency/.

The indexes must support Simple API as described here:
https://packaging.python.org/en/latest/specifications/simple-repository-api/
""",
        ),
        "hub_name": attr.string(
            mandatory = True,
            doc = """
The name of the repo pip dependencies will be accessible from.

This name must be unique between modules; unless your module is guaranteed to
always be the root module, it's highly recommended to include your module name
in the hub name. Repo mapping, `use_repo(..., pip="my_modules_pip_deps")`, can
be used for shorter local names within your module.

Within a module, the same `hub_name` can be specified to group different Python
versions of pip dependencies under one repository name. This allows using a
Python version-agnostic name when referring to pip dependencies; the
correct version will be automatically selected.

Typically, a module will only have a single hub of pip dependencies, but this
is not required. Each hub is a separate resolution of pip dependencies. This
means if different programs need different versions of some library, separate
hubs can be created, and each program can use its respective hub's targets.
Targets from different hubs should not be used together.
""",
        ),
        "parallel_download": attr.bool(
            doc = """\
The flag allows to make use of parallel downloading feature in bazel 7.1 and above
when the bazel downloader is used. This is by default enabled as it improves the
performance by a lot, but in case the queries to the simple API are very expensive
or when debugging authentication issues one may want to disable this feature.

NOTE, This will download (potentially duplicate) data for multiple packages if
there is more than one index available, but in general this should be negligible
because the simple API calls are very cheap and the user should not notice any
extra overhead.

If we are in synchronous mode, then we will use the first result that we
find in case extra indexes are specified.
""",
            default = True,
        ),
        "python_version": attr.string(
            mandatory = True,
            doc = """
The Python version the dependencies are targetting, in Major.Minor format
(e.g., "3.11") or patch level granularity (e.g. "3.11.1").

If an interpreter isn't explicitly provided (using `python_interpreter` or
`python_interpreter_target`), then the version specified here must have
a corresponding `python.toolchain()` configured.
""",
        ),
        "whl_modifications": attr.label_keyed_string_dict(
            mandatory = False,
            doc = """\
A dict of labels to wheel names that is typically generated by the whl_modifications.
The labels are JSON config files describing the modifications.
""",
        ),
        "_evaluate_markers_srcs": attr.label_list(
            default = EVALUATE_MARKERS_SRCS,
            doc = """\
The list of labels to use as SRCS for the marker evaluation code. This ensures that the
code will be re-evaluated when any of files in the default changes.
""",
        ),
    }, **ATTRS)
    attrs.update(AUTH_ATTRS)

    return attrs

def _whl_mod_attrs():
    attrs = {
        "additive_build_content": attr.string(
            doc = "(str, optional): Raw text to add to the generated `BUILD` file of a package.",
        ),
        "additive_build_content_file": attr.label(
            doc = """\
(label, optional): path to a BUILD file to add to the generated
`BUILD` file of a package. You cannot use both additive_build_content and additive_build_content_file
arguments at the same time.""",
        ),
        "copy_executables": attr.string_dict(
            doc = """\
(dict, optional): A mapping of `src` and `out` files for
[@bazel_skylib//rules:copy_file.bzl][cf]. Targets generated here will also be flagged as
executable.""",
        ),
        "copy_files": attr.string_dict(
            doc = """\
(dict, optional): A mapping of `src` and `out` files for
[@bazel_skylib//rules:copy_file.bzl][cf]""",
        ),
        "data": attr.string_list(
            doc = """\
(list, optional): A list of labels to add as `data` dependencies to
the generated `py_library` target.""",
        ),
        "data_exclude_glob": attr.string_list(
            doc = """\
(list, optional): A list of exclude glob patterns to add as `data` to
the generated `py_library` target.""",
        ),
        "hub_name": attr.string(
            doc = """\
Name of the whl modification, hub we use this name to set the modifications for
pip.parse. If you have different pip hubs you can use a different name,
otherwise it is best practice to just use one.

You cannot have the same `hub_name` in different modules.  You can reuse the same
name in the same module for different wheels that you put in the same hub, but you
cannot have a child module that uses the same `hub_name`.
""",
            mandatory = True,
        ),
        "srcs_exclude_glob": attr.string_list(
            doc = """\
(list, optional): A list of labels to add as `srcs` to the generated
`py_library` target.""",
        ),
        "whl_name": attr.string(
            doc = "The whl name that the modifications are used for.",
            mandatory = True,
        ),
    }
    return attrs

# NOTE: the naming of 'override' is taken from the bzlmod native
# 'archive_override', 'git_override' bzlmod functions.
_override_tag = tag_class(
    attrs = {
        "file": attr.string(
            doc = """\
The Python distribution file name which needs to be patched. This will be
applied to all repositories that setup this distribution via the pip.parse tag
class.""",
            mandatory = True,
        ),
        "patch_strip": attr.int(
            default = 0,
            doc = """\
The number of leading path segments to be stripped from the file name in the
patches.""",
        ),
        "patches": attr.label_list(
            doc = """\
A list of patches to apply to the repository *after* 'whl_library' is extracted
and BUILD.bazel file is generated.""",
            mandatory = True,
        ),
    },
    doc = """\
Apply any overrides (e.g. patches) to a given Python distribution defined by
other tags in this extension.""",
)

pypi = module_extension(
    doc = """\
This extension is used to make dependencies from pip available.

pip.parse:
To use, call `pip.parse()` and specify `hub_name` and your requirements file.
Dependencies will be downloaded and made available in a repo named after the
`hub_name` argument.

Each `pip.parse()` call configures a particular Python version. Multiple calls
can be made to configure different Python versions, and will be grouped by
the `hub_name` argument. This allows the same logical name, e.g. `@pip//numpy`
to automatically resolve to different, Python version-specific, libraries.

pip.whl_mods:
This tag class is used to help create JSON files to describe modifications to
the BUILD files for wheels.
""",
    implementation = _pip_impl,
    tag_classes = {
        "override": _override_tag,
        "parse": tag_class(
            attrs = _pip_parse_ext_attrs(),
            doc = """\
This tag class is used to create a pip hub and all of the spokes that are part of that hub.
This tag class reuses most of the attributes found in {bzl:obj}`pip_parse`.
The exception is it does not use the arg 'repo_prefix'.  We set the repository
prefix for the user and the alias arg is always True in bzlmod.
""",
        ),
        "whl_mods": tag_class(
            attrs = _whl_mod_attrs(),
            doc = """\
This tag class is used to create JSON file that are used when calling wheel_builder.py.  These
JSON files contain instructions on how to modify a wheel's project.  Each of the attributes
create different modifications based on the type of attribute. Previously to bzlmod these
JSON files where referred to as annotations, and were renamed to whl_modifications in this
extension.
""",
        ),
    },
)

def _whl_mods_repo_impl(rctx):
    rctx.file("BUILD.bazel", "")
    for whl_name, mods in rctx.attr.whl_mods.items():
        rctx.file("{}.json".format(whl_name), mods)

_whl_mods_repo = repository_rule(
    doc = """\
This rule creates json files based on the whl_mods attribute.
""",
    implementation = _whl_mods_repo_impl,
    attrs = {
        "whl_mods": attr.string_dict(
            mandatory = True,
            doc = "JSON endcoded string that is provided to wheel_builder.py",
        ),
    },
)
