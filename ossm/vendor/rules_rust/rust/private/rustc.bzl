# Copyright 2018 The Bazel Authors. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Functionality for constructing actions that invoke the Rust compiler"""

load("@bazel_skylib//rules:common_settings.bzl", "BuildSettingInfo")
load(
    "@bazel_tools//tools/build_defs/cc:action_names.bzl",
    "CPP_LINK_DYNAMIC_LIBRARY_ACTION_NAME",
    "CPP_LINK_EXECUTABLE_ACTION_NAME",
    "CPP_LINK_NODEPS_DYNAMIC_LIBRARY_ACTION_NAME",
    "CPP_LINK_STATIC_LIBRARY_ACTION_NAME",
)
load("@rules_cc//cc/common:cc_common.bzl", "cc_common")
load("@rules_cc//cc/common:cc_info.bzl", "CcInfo")
load(":common.bzl", "rust_common")
load(":compat.bzl", "abs")
load(":lto.bzl", "construct_lto_arguments")
load(
    ":providers.bzl",
    "AllocatorLibrariesImplInfo",
    "AllocatorLibrariesInfo",
    "AlwaysEnableMetadataOutputGroupsInfo",
    "LintsInfo",
    "RustcOutputDiagnosticsInfo",
    _BuildInfo = "BuildInfo",
)
load(":rustc_resource_set.bzl", "get_rustc_resource_set", "is_codegen_units_enabled")
load(":stamp.bzl", "is_stamping_enabled")
load(
    ":utils.bzl",
    "expand_dict_value_locations",
    "expand_list_element_locations",
    "find_cc_toolchain",
    "get_lib_name_default",
    "get_lib_name_for_windows",
    "get_preferred_artifact",
    "is_exec_configuration",
    "is_std_dylib",
    "make_static_lib_symlink",
    "parse_env_strings",
    "relativize",
)

# This feature is disabled unless one of the dependencies is a cc_library.
# Authors of C++ toolchains can place linker flags that should only be applied
# when linking with C objects in a feature with this name, or require this
# feature from other features which needs to be disabled together.
RUST_LINK_CC_FEATURE = "rules_rust_link_cc"

BuildInfo = _BuildInfo

AliasableDepInfo = provider(
    doc = "A provider mapping an alias name to a Crate's information.",
    fields = {
        "dep": "CrateInfo",
        "name": "str",
    },
)

_error_format_values = ["human", "json", "short"]

ErrorFormatInfo = provider(
    doc = "Set the --error-format flag for all rustc invocations",
    fields = {"error_format": "(string) [" + ", ".join(_error_format_values) + "]"},
)

ExtraRustcEnvInfo = provider(
    doc = "Pass each value as an environment variable to non-exec rustc invocations",
    fields = {"extra_rustc_env": "List[string] Extra env to pass to rustc in non-exec configuration"},
)

ExtraExecRustcEnvInfo = provider(
    doc = "Pass each value as an environment variable to exec rustc invocations",
    fields = {"extra_exec_rustc_env": "List[string] Extra env to pass to rustc in exec configuration"},
)

ExtraRustcFlagsInfo = provider(
    doc = "Pass each value as an additional flag to non-exec rustc invocations",
    fields = {"extra_rustc_flags": "List[string] Extra flags to pass to rustc in non-exec configuration"},
)

ExtraExecRustcFlagsInfo = provider(
    doc = "Pass each value as an additional flag to exec rustc invocations",
    fields = {"extra_exec_rustc_flags": "List[string] Extra flags to pass to rustc in exec configuration"},
)

PerCrateRustcFlagsInfo = provider(
    doc = "Pass each value as an additional flag to non-exec rustc invocations for crates matching the provided filter",
    fields = {"per_crate_rustc_flags": "List[string] Extra flags to pass to rustc in non-exec configuration"},
)

IsProcMacroDepInfo = provider(
    doc = "Records if this is a transitive dependency of a proc-macro.",
    fields = {"is_proc_macro_dep": "Boolean"},
)

def _is_proc_macro_dep_impl(ctx):
    return IsProcMacroDepInfo(is_proc_macro_dep = ctx.build_setting_value)

is_proc_macro_dep = rule(
    doc = "Records if this is a transitive dependency of a proc-macro.",
    implementation = _is_proc_macro_dep_impl,
    build_setting = config.bool(flag = True),
)

IsProcMacroDepEnabledInfo = provider(
    doc = "Enables the feature to record if a library is a transitive dependency of a proc-macro.",
    fields = {"enabled": "Boolean"},
)

def _is_proc_macro_dep_enabled_impl(ctx):
    return IsProcMacroDepEnabledInfo(enabled = ctx.build_setting_value)

is_proc_macro_dep_enabled = rule(
    doc = "Enables the feature to record if a library is a transitive dependency of a proc-macro.",
    implementation = _is_proc_macro_dep_enabled_impl,
    build_setting = config.bool(flag = True),
)

def _get_rustc_env(attr, toolchain, crate_name):
    """Gathers rustc environment variables

    Args:
        attr (struct): The current target's attributes
        toolchain (rust_toolchain): The current target's rust toolchain context
        crate_name (str): The name of the crate to be compiled

    Returns:
        dict: Rustc environment variables
    """
    version = attr.version if hasattr(attr, "version") else "0.0.0"
    major, minor, patch = version.split(".", 2)
    if "-" in patch:
        patch, pre = patch.split("-", 1)
    else:
        pre = ""

    result = {
        "CARGO_CFG_TARGET_ARCH": "" if toolchain.target_arch == None else toolchain.target_arch,
        "CARGO_CFG_TARGET_OS": "" if toolchain.target_os == None else toolchain.target_os,
        "CARGO_CRATE_NAME": crate_name,
        "CARGO_PKG_AUTHORS": "",
        "CARGO_PKG_DESCRIPTION": "",
        "CARGO_PKG_HOMEPAGE": "",
        "CARGO_PKG_NAME": attr.name,
        "CARGO_PKG_VERSION": version,
        "CARGO_PKG_VERSION_MAJOR": major,
        "CARGO_PKG_VERSION_MINOR": minor,
        "CARGO_PKG_VERSION_PATCH": patch,
        "CARGO_PKG_VERSION_PRE": pre,
    }
    if hasattr(attr, "_is_proc_macro_dep_enabled") and attr._is_proc_macro_dep_enabled[IsProcMacroDepEnabledInfo].enabled:
        is_proc_macro_dep = "0"
        if hasattr(attr, "_is_proc_macro_dep") and attr._is_proc_macro_dep[IsProcMacroDepInfo].is_proc_macro_dep:
            is_proc_macro_dep = "1"
        result["BAZEL_RULES_RUST_IS_PROC_MACRO_DEP"] = is_proc_macro_dep
    return result

def get_compilation_mode_opts(ctx, toolchain):
    """Gathers rustc flags for the current compilation mode (opt/debug)

    Args:
        ctx (ctx): The current rule's context object
        toolchain (rust_toolchain): The current rule's `rust_toolchain`

    Returns:
        struct: See `_rust_toolchain_impl` for more details
    """
    comp_mode = ctx.var["COMPILATION_MODE"]
    if not comp_mode in toolchain.compilation_mode_opts:
        fail("Unrecognized compilation mode {} for toolchain.".format(comp_mode))

    return toolchain.compilation_mode_opts[comp_mode]

def _are_linkstamps_supported(feature_configuration):
    # Are linkstamps supported by the C++ toolchain?
    return (cc_common.is_enabled(feature_configuration = feature_configuration, feature_name = "linkstamps") and
            # Is Bazel recent enough to support Starlark linkstamps?
            hasattr(cc_common, "register_linkstamp_compile_action"))

def _should_use_pic(cc_toolchain, feature_configuration, crate_type, compilation_mode):
    """Whether or not [PIC][pic] should be enabled

    [pic]: https://en.wikipedia.org/wiki/Position-independent_code

    Args:
        cc_toolchain (CcToolchainInfo): The current `cc_toolchain`.
        feature_configuration (FeatureConfiguration): Feature configuration to be queried.
        crate_type (str): A Rust target's crate type.
        compilation_mode: The compilation mode.

    Returns:
        bool: Whether or not [PIC][pic] should be enabled.
    """

    # We use the same logic to select between `pic` and `nopic` outputs as the C++ rules:
    # - For shared libraries - we use `pic`. This covers `dylib`, `cdylib` and `proc-macro` crate types.
    # - In `fastbuild` and `dbg` mode we use `pic` by default.
    # - In `opt` mode we use `nopic` outputs to build binaries.
    if cc_toolchain and crate_type in ("cdylib", "dylib", "proc-macro"):
        return cc_toolchain.needs_pic_for_dynamic_libraries(feature_configuration = feature_configuration)
    elif compilation_mode in ("fastbuild", "dbg"):
        return True
    return False

def _is_proc_macro(crate_info):
    return "proc-macro" in (crate_info.type, crate_info.wrapped_crate_type)

def collect_deps(
        deps,
        proc_macro_deps,
        aliases):
    """Walks through dependencies and collects the transitive dependencies.

    Args:
        deps (list): The deps from ctx.attr.deps.
        proc_macro_deps (list): The proc_macro deps from ctx.attr.proc_macro_deps.
        aliases (dict): A dict mapping aliased targets to their actual Crate information.

    Returns:
        tuple: Returns a tuple of:
            DepInfo,
            BuildInfo,
            linkstamps (depset[CcLinkstamp]): A depset of CcLinkstamps that need to be compiled and linked into all linked binaries when applicable.

    """
    direct_deps = []

    direct_crates = []
    transitive_crates = []

    transitive_data = []
    transitive_proc_macro_data = []
    transitive_noncrates = []

    direct_build_infos = []
    transitive_build_infos = []

    direct_link_search_paths = []
    transitive_link_search_paths = []

    build_info = None
    linkstamps = []

    direct_crate_outputs = []
    transitive_crate_outputs = []

    direct_metadata_outputs = []
    transitive_metadata_outputs = []

    crate_deps = []
    for dep in deps + proc_macro_deps:
        crate_group = getattr(dep, "crate_group_info", None)
        if crate_group:
            crate_deps.extend(crate_group.dep_variant_infos.to_list())
        else:
            crate_deps.append(dep)

    aliases = {k.label: v for k, v in aliases.items()}
    for dep in crate_deps:
        crate_info = dep.crate_info
        dep_info = dep.dep_info
        cc_info = dep.cc_info
        dep_build_info = dep.build_info

        if cc_info:
            for li in cc_info.linking_context.linker_inputs.to_list():
                linkstamps.extend(li.linkstamps)

        if crate_info:
            # This dependency is a rust_library
            direct_deps.append(AliasableDepInfo(
                name = aliases.get(crate_info.owner, crate_info.name),
                dep = crate_info,
            ))

            is_proc_macro = _is_proc_macro(crate_info)

            direct_crates.append(crate_info)
            if not is_proc_macro:
                transitive_crates.append(dep_info.transitive_crates)

            if is_proc_macro:
                # This crate's data and its non-macro dependencies' data are proc macro data.
                transitive_proc_macro_data.append(crate_info.data)
                transitive_proc_macro_data.append(dep_info.transitive_data)
            else:
                # This crate's proc macro dependencies' data are proc macro data.
                transitive_proc_macro_data.append(dep_info.transitive_proc_macro_data)

                # Track transitive non-macro data in case a proc macro depends on this crate.
                transitive_data.append(crate_info.data)
                transitive_data.append(dep_info.transitive_data)

            # If this dependency produces metadata, add it to the metadata outputs.
            # If it doesn't (for example a custom library that exports crate_info),
            # we depend on crate_info.output.
            depend_on = crate_info.metadata
            if not crate_info.metadata or not crate_info.metadata_supports_pipelining:
                depend_on = crate_info.output

            # If this dependency is a proc_macro, it still can be used for lib crates
            # that produce metadata.
            # In that case, we don't depend on its metadata dependencies.
            direct_metadata_outputs.append(depend_on)
            if not is_proc_macro:
                transitive_metadata_outputs.append(dep_info.transitive_metadata_outputs)

            direct_crate_outputs.append(crate_info.output)
            if not is_proc_macro:
                transitive_crate_outputs.append(dep_info.transitive_crate_outputs)

            if not is_proc_macro:
                transitive_noncrates.append(dep_info.transitive_noncrates)
                transitive_link_search_paths.append(dep_info.link_search_path_files)

            transitive_build_infos.append(dep_info.transitive_build_infos)
        elif cc_info or dep_build_info:
            if cc_info:
                # This dependency is a cc_library
                transitive_noncrates.append(cc_info.linking_context.linker_inputs)

            if dep_build_info:
                if build_info:
                    fail("Several deps are providing build information, " +
                         "only one is allowed in the dependencies")
                build_info = dep_build_info
                direct_build_infos.append(build_info)
                if build_info.link_search_paths:
                    direct_link_search_paths.append(build_info.link_search_paths)
                transitive_data.append(build_info.compile_data)
        else:
            fail("rust targets can only depend on rust_library, rust_*_library or cc_library " +
                 "targets.")

    return (
        rust_common.dep_info(
            direct_crates = depset(direct_deps),
            transitive_crates = depset(
                direct_crates,
                transitive = transitive_crates,
            ),
            transitive_data = depset(
                transitive = transitive_data,
            ),
            transitive_proc_macro_data = depset(
                transitive = transitive_proc_macro_data,
            ),
            transitive_noncrates = depset(
                transitive = transitive_noncrates,
                order = "topological",  # dylib link flag ordering matters.
            ),
            transitive_crate_outputs = depset(
                direct_crate_outputs,
                transitive = transitive_crate_outputs,
            ),
            transitive_metadata_outputs = depset(
                direct_metadata_outputs,
                transitive = transitive_metadata_outputs,
            ),
            transitive_build_infos = depset(
                direct_build_infos,
                transitive = transitive_build_infos,
            ),
            link_search_path_files = depset(
                direct_link_search_paths,
                transitive = transitive_link_search_paths,
            ),
            dep_env = build_info.dep_env if build_info else None,
        ),
        build_info,
        depset(linkstamps),
    )

def _collect_libs_from_linker_inputs(linker_inputs, use_pic):
    # TODO: We could let the user choose how to link, instead of always preferring to link static libraries.
    return [
        get_preferred_artifact(lib, use_pic)
        for li in linker_inputs
        for lib in li.libraries
    ]

def get_cc_user_link_flags(ctx):
    """Get the current target's linkopt flags

    Args:
        ctx (ctx): The current rule's context object

    Returns:
        depset: The flags passed to Bazel by --linkopt option.
    """
    return ctx.fragments.cpp.linkopts

def get_linker_and_args(ctx, crate_type, toolchain, cc_toolchain, feature_configuration, rpaths, add_flags_for_binary = False):
    """Gathers cc_common linker information

    Args:
        ctx (ctx): The current target's context object
        crate_type (str): The target crate's type (i.e. "bin", "proc-macro", etc.).
        toolchain (rust_toolchain): The current Rust toolchain.
        cc_toolchain (CcToolchain): cc_toolchain for which we are creating build variables.
        feature_configuration (FeatureConfiguration): Feature configuration to be queried.
        rpaths (depset): Depset of directories where loader will look for libraries at runtime.
        add_flags_for_binary (bool, optional): Whether to add "bin" link flags to the command regardless of `crate_type`.


    Returns:
        tuple: A tuple of the following items:
            - (str): The tool path for given action.
            - (bool): Whether or not the linker is a direct driver (e.g. `ld`) vs a wrapper (e.g. `gcc`).
            - (sequence): A flattened command line flags for given action.
            - (dict): Environment variables to be set for given action.
    """
    user_link_flags = get_cc_user_link_flags(ctx)

    ld = None
    ld_is_direct_driver = False
    link_args = []
    link_env = {}

    if cc_toolchain:
        if crate_type in ("bin") or add_flags_for_binary:
            is_linking_dynamic_library = False
            action_name = CPP_LINK_EXECUTABLE_ACTION_NAME
        elif crate_type in ("dylib"):
            is_linking_dynamic_library = True
            action_name = CPP_LINK_NODEPS_DYNAMIC_LIBRARY_ACTION_NAME
        elif crate_type in ("staticlib"):
            is_linking_dynamic_library = False
            action_name = CPP_LINK_STATIC_LIBRARY_ACTION_NAME
        elif crate_type in ("cdylib", "proc-macro"):
            # Proc macros get compiled as shared libraries to be loaded by the compiler.
            is_linking_dynamic_library = True
            action_name = CPP_LINK_DYNAMIC_LIBRARY_ACTION_NAME
        elif crate_type in ("lib", "rlib"):
            fail("Invalid `crate_type` for linking action: {}".format(crate_type))
        else:
            fail("Unknown `crate_type`: {}".format(crate_type))

        link_variables = cc_common.create_link_variables(
            feature_configuration = feature_configuration,
            cc_toolchain = cc_toolchain,
            is_linking_dynamic_library = is_linking_dynamic_library,
            runtime_library_search_directories = rpaths,
            user_link_flags = user_link_flags,
        )
        link_args.extend(cc_common.get_memory_inefficient_command_line(
            feature_configuration = feature_configuration,
            action_name = action_name,
            variables = link_variables,
        ))
        link_env = cc_common.get_environment_variables(
            feature_configuration = feature_configuration,
            action_name = action_name,
            variables = link_variables,
        )
        ld = cc_common.get_tool_for_action(
            feature_configuration = feature_configuration,
            action_name = action_name,
        )
        ld_is_direct_driver = False

    if not ld or toolchain.linker_preference == "rust":
        ld = toolchain.linker.path
        ld_is_direct_driver = toolchain.linker_type == "direct"

        # When using rust-lld directly, we still need library search paths from cc_toolchain
        # to find system libraries that rustc's stdlib depends on (like -lgcc_s, -lutil, etc.)
        # Filter link_args to only include flags that help locate libraries.
        if cc_toolchain and link_args:
            filtered_args = []
            skip_next = False
            for i, arg in enumerate(link_args):
                if skip_next:
                    skip_next = False
                    continue

                # Strip -Wl, prefix if using direct driver (it's only for compiler drivers)
                processed_arg = arg
                if ld_is_direct_driver and arg.startswith("-Wl,"):
                    # Remove -Wl, prefix and split on commas (e.g., "-Wl,-rpath,/path" -> ["-rpath", "/path"])
                    # For now, we'll handle common cases; complex -Wl, args might need more sophisticated handling
                    processed_arg = arg[4:]  # Strip "-Wl,"

                # Handle macOS version flag: convert -mmacos-version-min=X.Y to -macos_version_min X.Y
                if processed_arg.startswith("-mmacos-version-min="):
                    version = processed_arg.split("=", 1)[1]
                    filtered_args.append("-macos_version_min")
                    filtered_args.append(version)
                    # Keep library search path flags

                elif processed_arg.startswith("-L"):
                    filtered_args.append(processed_arg)
                    # Keep sysroot flags (as single or two-part arguments)

                elif processed_arg == "--sysroot" or processed_arg.startswith("--sysroot="):
                    filtered_args.append(processed_arg)
                    if processed_arg == "--sysroot" and i + 1 < len(link_args):
                        # Two-part argument, keep the next arg too
                        filtered_args.append(link_args[i + 1])
                        skip_next = True

                    # Keep dynamic linker flags
                elif processed_arg.startswith("--dynamic-linker") or processed_arg == "--dynamic-linker":
                    filtered_args.append(processed_arg)
                    if processed_arg == "--dynamic-linker" and i + 1 < len(link_args):
                        filtered_args.append(link_args[i + 1])
                        skip_next = True

                    # Keep rpath-related flags
                elif processed_arg.startswith("-rpath") or processed_arg.startswith("--rpath"):
                    filtered_args.append(processed_arg)
            link_args = filtered_args

    if not ld:
        fail("No linker available for rustc. Either `rust_toolchain.linker` must be set or a `cc_toolchain` configured for the current configuration.")

    if "LIB" in link_env:
        # Needed to ensure that link.exe will use msvcrt.lib from the cc_toolchain,
        # and not a non-hermetic system version.
        # https://github.com/bazelbuild/rules_rust/issues/3256
        # I don't see a good way to stop rustc from adding the non-hermetic library search path,
        # so put our cc_toolchain library search path on the command line where it has
        # precedence over the non-hermetic path injected by rustc.
        link_args.extend([
            "-LIBPATH:" + element
            for element in link_env["LIB"].split(";")
        ])

    return ld, ld_is_direct_driver, link_args, link_env

def _symlink_for_ambiguous_lib(actions, toolchain, crate_info, lib):
    """Constructs a disambiguating symlink for a library dependency.

    Args:
      actions (Actions): The rule's context actions object.
      toolchain: The Rust toolchain object.
      crate_info (CrateInfo): The target crate's info.
      lib (File): The library to symlink to.

    Returns:
      (File): The disambiguating symlink for the library.
    """
    # FIXME: Once the relative order part of the native-link-modifiers rustc
    # feature is stable, we should be able to eliminate the need to construct
    # symlinks by passing the full paths to the libraries.
    # https://github.com/rust-lang/rust/issues/81490.

    # Take the absolute value of hash() since it could be negative.
    path_hash = abs(hash(lib.path))
    lib_name = get_lib_name_for_windows(lib) if toolchain.target_os.startswith("windows") else get_lib_name_default(lib)

    if toolchain.target_os.startswith("windows"):
        prefix = ""
        extension = ".lib"
    elif lib_name.endswith(".pic"):
        # Strip the .pic suffix
        lib_name = lib_name[:-4]
        prefix = "lib"
        extension = ".pic.a"
    else:
        prefix = "lib"
        extension = ".a"

    # Ensure the symlink follows the lib<name>.a pattern on Unix-like platforms
    # or <name>.lib on Windows.
    # Add a hash of the original library path to disambiguate libraries with the same basename.
    symlink_name = "{}{}-{}{}".format(prefix, lib_name, path_hash, extension)

    # Add the symlink to a target crate-specific _ambiguous_libs/ subfolder,
    # to avoid possible collisions with sibling crates that may depend on the
    # same ambiguous libraries.
    symlink = actions.declare_file("_ambiguous_libs/" + crate_info.output.basename + "/" + symlink_name)
    actions.symlink(
        output = symlink,
        target_file = lib,
        progress_message = "Creating symlink to ambiguous lib: {}".format(lib.path),
    )
    return symlink

def _disambiguate_libs(actions, toolchain, crate_info, dep_info, use_pic):
    """Constructs disambiguating symlinks for ambiguous library dependencies.

    The symlinks are all created in a _ambiguous_libs/ subfolder specific to
    the target crate to avoid possible collisions with sibling crates that may
    depend on the same ambiguous libraries.

    Args:
      actions (Actions): The rule's context actions object.
      toolchain: The Rust toolchain object.
      crate_info (CrateInfo): The target crate's info.
      dep_info: (DepInfo): The target crate's dependency info.
      use_pic: (boolean): Whether the build should use PIC.

    Returns:
      dict[String, File]: A mapping from ambiguous library paths to their
        disambiguating symlink.
    """
    # FIXME: Once the relative order part of the native-link-modifiers rustc
    # feature is stable, we should be able to eliminate the need to construct
    # symlinks by passing the full paths to the libraries.
    # https://github.com/rust-lang/rust/issues/81490.

    # A dictionary from file paths of ambiguous libraries to the corresponding
    # symlink.
    ambiguous_libs = {}

    # A dictionary maintaining a mapping from preferred library name to the
    # last visited artifact with that name.
    visited_libs = {}
    for link_input in dep_info.transitive_noncrates.to_list():
        for lib in link_input.libraries:
            # FIXME: Dynamic libs are not disambiguated right now, there are
            # cases where those have a non-standard name with version (e.g.,
            # //test/unit/versioned_libs). We hope that the link modifiers
            # stabilization will come before we need to make this work.
            if _is_dylib(lib):
                continue
            artifact = get_preferred_artifact(lib, use_pic)
            name = get_lib_name_for_windows(artifact) if toolchain.target_os.startswith("windows") else get_lib_name_default(artifact)

            # On Linux-like platforms, normally library base names start with
            # `lib`, following the pattern `lib[name].(a|lo)` and we pass
            # -lstatic=name.
            # On Windows, the base name looks like `name.lib` and we pass
            # -lstatic=name.
            # FIXME: Under the native-link-modifiers unstable rustc feature,
            # we could use -lstatic:+verbatim instead.
            needs_symlink_to_standardize_name = (
                toolchain.target_os.startswith(("linux", "mac", "darwin")) and
                artifact.basename.endswith(".a") and not artifact.basename.startswith("lib")
            ) or (
                toolchain.target_os.startswith("windows") and not artifact.basename.endswith(".lib")
            )

            # Detect cases where we need to disambiguate library dependencies
            # by constructing symlinks.
            if (
                needs_symlink_to_standardize_name or
                # We have multiple libraries with the same name.
                (name in visited_libs and visited_libs[name].path != artifact.path)
            ):
                # Disambiguate the previously visited library (if we just detected
                # that it is ambiguous) and the current library.
                if name in visited_libs:
                    old_path = visited_libs[name].path
                    if old_path not in ambiguous_libs:
                        ambiguous_libs[old_path] = _symlink_for_ambiguous_lib(actions, toolchain, crate_info, visited_libs[name])
                ambiguous_libs[artifact.path] = _symlink_for_ambiguous_lib(actions, toolchain, crate_info, artifact)

            visited_libs[name] = artifact
    return ambiguous_libs

def _depend_on_metadata(crate_info, force_depend_on_objects):
    """Determines if we can depend on metadata for this crate.

    By default (when pipelining is disabled or when the crate type needs to link against
    objects) we depend on the set of object files (.rlib).
    When pipelining is enabled and the crate type supports depending on metadata,
    we depend on metadata files only (.rmeta).
    In some rare cases, even if both of those conditions are true, we still want to
    depend on objects. This is what force_depend_on_objects is.

    Args:
        crate_info (CrateInfo): The Crate to determine this for.
        force_depend_on_objects (bool): if set we will not depend on metadata.

    Returns:
        Whether we can depend on metadata for this crate.
    """
    if force_depend_on_objects:
        return False

    return crate_info.type in ("rlib", "lib")

def collect_inputs(
        ctx,
        file,
        files,
        linkstamps,
        toolchain,
        cc_toolchain,
        feature_configuration,
        crate_info,
        dep_info,
        build_info,
        lint_files,
        stamp = False,
        force_depend_on_objects = False,
        experimental_use_cc_common_link = False,
        include_link_flags = True):
    """Gather's the inputs and required input information for a rustc action

    Args:
        ctx (ctx): The rule's context object.
        file (struct): A struct containing files defined in label type attributes marked as `allow_single_file`.
        files (struct): A struct of all inputs (`ctx.files`). When aspects are involved, the rule context
            may not correspond to a rust target, so check that files attributes are present before accessing them.
        linkstamps (depset): A depset of CcLinkstamps that need to be compiled and linked into all linked binaries.
        toolchain (rust_toolchain): The current `rust_toolchain`.
        cc_toolchain (CcToolchainInfo): The current `cc_toolchain`.
        feature_configuration (FeatureConfiguration): Feature configuration to be queried.
        crate_info (CrateInfo): The Crate information of the crate to process build scripts for.
        dep_info (DepInfo): The target Crate's dependency information.
        build_info (BuildInfo): The target Crate's build settings.
        lint_files (list): List of files with rustc args for the Crate's lint settings.
        stamp (bool, optional): Whether or not workspace status stamping is enabled. For more details see
            https://docs.bazel.build/versions/main/user-manual.html#flag--stamp
        force_depend_on_objects (bool, optional): Forces dependencies of this rule to be objects rather than
            metadata, even for libraries. This is used in rustdoc tests.
        experimental_use_cc_common_link (bool, optional): Whether rules_rust uses cc_common.link to link
            rust binaries.
        include_link_flags (bool, optional): Whether to include flags like `-l` that instruct the linker to search for a library.

    Returns:
        tuple: A tuple: A tuple of the following items:
            - (list): A list of all build info `OUT_DIR` File objects
            - (str): The `OUT_DIR` of the current build info
            - (File): An optional path to a generated environment file from a `cargo_build_script` target
            - (depset[File]): All direct and transitive build flag files from the current build info
            - (list[File]): Linkstamp outputs
            - (dict[String, File]): Ambiguous libs, see `_disambiguate_libs`.
    """
    linker_script = getattr(file, "linker_script", None)

    # TODO: As of writing this comment Bazel used Java CcToolchainInfo.
    # However there is ongoing work to rewrite provider in Starlark.
    # rules_rust is not coupled with Bazel release. Remove conditional and change to
    # _linker_files once Starlark CcToolchainInfo is visible to Bazel.
    # https://github.com/bazelbuild/rules_rust/issues/2425
    if not cc_toolchain:
        linker_depset = depset()
    elif hasattr(cc_toolchain, "_linker_files"):
        linker_depset = cc_toolchain._linker_files
    else:
        linker_depset = cc_toolchain.linker_files()
    compilation_mode = ctx.var["COMPILATION_MODE"]

    use_pic = _should_use_pic(cc_toolchain, feature_configuration, crate_info.type, compilation_mode)

    # Pass linker inputs only for linking-like actions, not for example where
    # the output is rlib. This avoids quadratic behavior where transitive noncrates are
    # flattened on each transitive rust_library dependency.
    libs_from_linker_inputs = []
    ambiguous_libs = {}
    if crate_info.type not in ("lib", "rlib"):
        linker_inputs = dep_info.transitive_noncrates.to_list()
        ambiguous_libs = _disambiguate_libs(ctx.actions, toolchain, crate_info, dep_info, use_pic)
        libs_from_linker_inputs = _collect_libs_from_linker_inputs(linker_inputs, use_pic) + [
            additional_input
            for linker_input in linker_inputs
            for additional_input in linker_input.additional_inputs
        ] + ambiguous_libs.values()

    # Compute linkstamps. Use the inputs of the binary as inputs to the
    # linkstamp action to ensure linkstamps are rebuilt whenever binary inputs
    # change.
    linkstamp_outs = []

    transitive_crate_outputs = dep_info.transitive_crate_outputs
    if _depend_on_metadata(crate_info, force_depend_on_objects):
        transitive_crate_outputs = dep_info.transitive_metadata_outputs

    nolinkstamp_compile_direct_inputs = []
    if build_info:
        if build_info.rustc_env:
            nolinkstamp_compile_direct_inputs.append(build_info.rustc_env)
        if build_info.flags:
            nolinkstamp_compile_direct_inputs.append(build_info.flags)

    # The old default behavior was to include data files at compile time.
    # This flag controls whether to include data files in compile_data.
    if not toolchain._incompatible_do_not_include_data_in_compile_data and hasattr(files, "data"):
        nolinkstamp_compile_direct_inputs += files.data

    if toolchain.target_json:
        nolinkstamp_compile_direct_inputs.append(toolchain.target_json)

    if linker_script:
        nolinkstamp_compile_direct_inputs.append(linker_script)

    if not cc_toolchain:
        runtime_libs = depset()
    elif crate_info.type in ["dylib", "cdylib"]:
        # For shared libraries we want to link C++ runtime library dynamically
        # (for example libstdc++.so or libc++.so).
        runtime_libs = cc_toolchain.dynamic_runtime_lib(feature_configuration = feature_configuration)
    else:
        runtime_libs = cc_toolchain.static_runtime_lib(feature_configuration = feature_configuration)

    nolinkstamp_compile_inputs = depset(
        nolinkstamp_compile_direct_inputs +
        ([] if experimental_use_cc_common_link else libs_from_linker_inputs),
        transitive = [
            crate_info.srcs,
            transitive_crate_outputs,
            crate_info.compile_data,
            dep_info.transitive_proc_macro_data,
            toolchain.all_files,
        ] + ([] if experimental_use_cc_common_link else [
            runtime_libs,
            linker_depset,
        ]),
    )

    # Register linkstamps when linking with rustc (when linking with
    # cc_common.link linkstamps are handled by cc_common.link itself).
    if not experimental_use_cc_common_link and crate_info.type in ("bin", "cdylib", "proc-macro"):
        # There is no other way to register an action for each member of a depset than
        # flattening the depset as of 2021-10-12. Luckily, usually there is only one linkstamp
        # in a build, and we only flatten the list on binary targets that perform transitive linking,
        # so it's extremely unlikely that this call to `to_list()` will ever be a performance
        # problem.
        for linkstamp in linkstamps.to_list():
            # The linkstamp output path is based on the binary crate
            # name and the input linkstamp path. This is to disambiguate
            # the linkstamp outputs produced by multiple binary crates
            # that depend on the same linkstamp. We use the same pattern
            # for the output name as the one used by native cc rules.
            out_name = "_objs/" + crate_info.output.basename + "/" + linkstamp.file().path[:-len(linkstamp.file().extension)] + "o"
            linkstamp_out = ctx.actions.declare_file(out_name)
            linkstamp_outs.append(linkstamp_out)
            cc_common.register_linkstamp_compile_action(
                actions = ctx.actions,
                cc_toolchain = cc_toolchain,
                feature_configuration = feature_configuration,
                source_file = linkstamp.file(),
                output_file = linkstamp_out,
                compilation_inputs = linkstamp.hdrs(),
                inputs_for_validation = nolinkstamp_compile_inputs,
                label_replacement = str(ctx.label),
                output_replacement = crate_info.output.path,
            )

    # If stamping is enabled include the volatile and stable status info file
    stamp_info = [ctx.version_file, ctx.info_file] if stamp else []

    compile_inputs = depset(
        linkstamp_outs + stamp_info,
        transitive = [
            nolinkstamp_compile_inputs,
        ],
    )

    build_script_compile_inputs, out_dir, build_env_file, build_flags_files = _process_build_scripts(
        build_info = build_info,
        dep_info = dep_info,
        include_link_flags = include_link_flags,
    )

    # TODO(parkmycar): Cleanup the handling of lint_files here.
    if lint_files:
        build_flags_files = depset(lint_files, transitive = [build_flags_files])

    # For backwards compatibility, we also check the value of the `rustc_env_files` attribute when
    # `crate_info.rustc_env_files` is not populated.
    build_env_files = crate_info.rustc_env_files if crate_info.rustc_env_files else getattr(files, "rustc_env_files", [])
    if build_env_file:
        build_env_files = list(build_env_files)
        build_env_files.append(build_env_file)
    compile_inputs = depset(build_env_files + lint_files, transitive = [build_script_compile_inputs, compile_inputs])
    return compile_inputs, out_dir, build_env_files, build_flags_files, linkstamp_outs, ambiguous_libs

def _will_emit_object_file(emit):
    for e in emit:
        if e == "obj" or e.startswith("obj="):
            return True
    return False

def _remove_codegen_units(flag):
    return None if flag.startswith("-Ccodegen-units") else flag

def construct_arguments(
        *,
        ctx,
        attr,
        file,
        toolchain,
        tool_path,
        cc_toolchain,
        feature_configuration,
        crate_info,
        dep_info,
        linkstamp_outs,
        ambiguous_libs,
        output_hash,
        rust_flags,
        out_dir,
        build_env_files,
        build_flags_files,
        emit = ["dep-info", "link"],
        force_all_deps_direct = False,
        add_flags_for_binary = False,
        include_link_flags = True,
        stamp = False,
        remap_path_prefix = ".",
        use_json_output = False,
        build_metadata = False,
        force_depend_on_objects = False,
        skip_expanding_rustc_env = False,
        require_explicit_unstable_features = False,
        error_format = None):
    """Builds an Args object containing common rustc flags

    Args:
        ctx (ctx): The rule's context object
        attr (struct): The attributes for the target. These may be different from ctx.attr in an aspect context.
        file (struct): A struct containing files defined in label type attributes marked as `allow_single_file`.
        toolchain (rust_toolchain): The current target's `rust_toolchain`
        tool_path (str): Path to rustc
        cc_toolchain (CcToolchain): The CcToolchain for the current target.
        feature_configuration (FeatureConfiguration): Class used to construct command lines from CROSSTOOL features.
        crate_info (CrateInfo): The CrateInfo provider of the target crate
        dep_info (DepInfo): The DepInfo provider of the target crate
        linkstamp_outs (list): Linkstamp outputs of native dependencies
        ambiguous_libs (dict): Ambiguous libs, see `_disambiguate_libs`
        output_hash (str): The hashed path of the crate root
        rust_flags (list): Additional flags to pass to rustc
        out_dir (str): The path to the output directory for the target Crate.
        build_env_files (list): Files containing rustc environment variables, for instance from `cargo_build_script` actions.
        build_flags_files (depset): The output files of a `cargo_build_script` actions containing rustc build flags
        emit (list): Values for the --emit flag to rustc.
        force_all_deps_direct (bool, optional): Whether to pass the transitive rlibs with --extern
            to the commandline as opposed to -L.
        add_flags_for_binary (bool, optional): Whether to add "bin" link flags to the command regardless of `emit` and `crate_type`.
        include_link_flags (bool, optional): Whether to include flags like `-l` that instruct the linker to search for a library.
        stamp (bool, optional): Whether or not workspace status stamping is enabled. For more details see
            https://docs.bazel.build/versions/main/user-manual.html#flag--stamp
        remap_path_prefix (str, optional): A value used to remap `${pwd}` to. If set to None, no prefix will be set.
        use_json_output (bool): Have rustc emit json and process_wrapper parse json messages to output rendered output.
        build_metadata (bool): Generate CLI arguments for building *only* .rmeta files. This requires use_json_output.
        force_depend_on_objects (bool): Force using `.rlib` object files instead of metadata (`.rmeta`) files even if they are available.
        skip_expanding_rustc_env (bool): Whether to skip expanding CrateInfo.rustc_env_attr
        require_explicit_unstable_features (bool): Whether to require all unstable features to be explicitly opted in to using `-Zallow-features=...`.
        error_format (str, optional): Error format to pass to the `--error-format` command line argument. If set to None, uses the "_error_format" entry in `attr`.

    Returns:
        tuple: A tuple of the following items
            - (struct): A struct of arguments used to run the `Rustc` action
                - process_wrapper_flags (Args): Arguments for the process wrapper
                - rustc_path (Args): Arguments for invoking rustc via the process wrapper
                - rustc_flags (Args): Rust flags for the Rust compiler
                - all (list): A list of all `Args` objects in the order listed above.
                    This is to be passed to the `arguments` parameter of actions
            - (dict): Common rustc environment variables
    """
    if build_metadata and not use_json_output:
        fail("build_metadata requires parse_json_output")

    output_dir = getattr(crate_info.output, "dirname", None)
    linker_script = getattr(file, "linker_script", None)

    env = _get_rustc_env(attr, toolchain, crate_info.name)

    # Wrapper args first
    process_wrapper_flags = ctx.actions.args()

    for build_env_file in build_env_files:
        process_wrapper_flags.add("--env-file", build_env_file)

    process_wrapper_flags.add_all(build_flags_files, before_each = "--arg-file")

    if require_explicit_unstable_features:
        process_wrapper_flags.add("--require-explicit-unstable-features", "true")

    # Certain rust build processes expect to find files from the environment
    # variable `$CARGO_MANIFEST_DIR`. Examples of this include pest, tera,
    # asakuma.
    #
    # The compiler and by extension proc-macros see the current working
    # directory as the Bazel exec root. This is what `$CARGO_MANIFEST_DIR`
    # would default to but is often the wrong value (e.g. if the source is in a
    # sub-package or if we are building something in an external repository).
    # Hence, we need to set `CARGO_MANIFEST_DIR` explicitly.
    #
    # Since we cannot get the `exec_root` from starlark, we cheat a little and
    # use `${pwd}` which resolves the `exec_root` at action execution time.
    process_wrapper_flags.add("--subst", "pwd=${pwd}")

    # If stamping is enabled, enable the functionality in the process wrapper
    if stamp:
        process_wrapper_flags.add("--volatile-status-file", ctx.version_file)
        process_wrapper_flags.add("--stable-status-file", ctx.info_file)

    # Both ctx.label.workspace_root and ctx.label.package are relative paths
    # and either can be empty strings. Avoid trailing/double slashes in the path.
    components = "${{pwd}}/{}/{}".format(ctx.label.workspace_root, ctx.label.package).split("/")
    env["CARGO_MANIFEST_DIR"] = "/".join([c for c in components if c])

    if out_dir != None:
        env["OUT_DIR"] = "${pwd}/" + out_dir

    # Arguments for launching rustc from the process wrapper
    rustc_path = ctx.actions.args()
    rustc_path.add("--")
    rustc_path.add(tool_path)

    # If we're emitting an object file, remove any `-Ccodegen-units=` flags.
    # The build rules expect to see a single object file, not the multiple
    # object files that are produced when `-Ccodegen-units` (with a setting
    # greater than 1) is used.
    map_flag = _remove_codegen_units if _will_emit_object_file(emit) else None

    # Rustc arguments
    rustc_flags = ctx.actions.args()
    rustc_flags.set_param_file_format("multiline")
    rustc_flags.use_param_file("@%s", use_always = False)
    rustc_flags.add(crate_info.root)
    rustc_flags.add(crate_info.name, format = "--crate-name=%s")
    rustc_flags.add(crate_info.type, format = "--crate-type=%s")

    if error_format == None:
        error_format = get_error_format(attr, "_error_format")

    if use_json_output:
        # If --error-format was set to json, we just pass the output through
        # Otherwise process_wrapper uses the "rendered" field.
        process_wrapper_flags.add("--rustc-output-format", "json" if error_format == "json" else "rendered")

        # Configure rustc json output by adding artifact notifications.
        # These will always be filtered out by process_wrapper and will be use to terminate
        # rustc when appropriate.
        json = ["artifacts"]
        if error_format == "short":
            json.append("diagnostic-short")
        elif error_format == "human" and toolchain.target_os != "windows":
            # If the os is not windows, we can get colorized output.
            json.append("diagnostic-rendered-ansi")

        rustc_flags.add_joined(json, format_joined = "--json=%s", join_with = ",")

        error_format = "json"

    if build_metadata:
        # Configure process_wrapper to terminate rustc when metadata are emitted
        process_wrapper_flags.add("--rustc-quit-on-rmeta", "true")
        if crate_info.rustc_rmeta_output:
            process_wrapper_flags.add("--output-file", crate_info.rustc_rmeta_output.path)
    elif crate_info.rustc_output:
        process_wrapper_flags.add("--output-file", crate_info.rustc_output.path)

    rustc_flags.add(error_format, format = "--error-format=%s")

    # Mangle symbols to disambiguate crates with the same name. This could
    # happen only for non-final artifacts where we compute an output_hash,
    # e.g., rust_library.
    #
    # For "final" artifacts and ones intended for distribution outside of
    # Bazel, such as rust_binary, rust_static_library and rust_shared_library,
    # where output_hash is None we don't need to add these flags.
    if output_hash:
        rustc_flags.add(output_hash, format = "--codegen=metadata=-%s")
        rustc_flags.add(output_hash, format = "--codegen=extra-filename=-%s")

    if output_dir:
        rustc_flags.add(output_dir, format = "--out-dir=%s")

    compilation_mode = get_compilation_mode_opts(ctx, toolchain)
    rustc_flags.add(compilation_mode.opt_level, format = "--codegen=opt-level=%s")
    rustc_flags.add(compilation_mode.debug_info, format = "--codegen=debuginfo=%s")
    rustc_flags.add(compilation_mode.strip_level, format = "--codegen=strip=%s")

    # For determinism to help with build distribution and such
    if remap_path_prefix != None:
        rustc_flags.add("--remap-path-prefix=${{pwd}}={}".format(remap_path_prefix))

    emit_without_paths = []
    for kind in emit:
        if kind == "link" and crate_info.type == "bin" and crate_info.output != None:
            rustc_flags.add(crate_info.output, format = "--emit=link=%s")
        else:
            emit_without_paths.append(kind)

    if emit_without_paths:
        rustc_flags.add_joined(emit_without_paths, format_joined = "--emit=%s", join_with = ",")
    if error_format != "json":
        # Color is not compatible with json output.
        rustc_flags.add("--color=always")
    rustc_flags.add(toolchain.target_flag_value, format = "--target=%s")
    if hasattr(attr, "crate_features"):
        rustc_flags.add_all(getattr(attr, "crate_features"), before_each = "--cfg", format_each = 'feature="%s"')
    if linker_script:
        rustc_flags.add(linker_script, format = "--codegen=link-arg=-T%s")

    # Tell Rustc where to find the standard library (or libcore)
    rustc_flags.add_all(toolchain.rust_std_paths, before_each = "-L", format_each = "%s")
    rustc_flags.add_all(rust_flags, map_each = map_flag)

    # Gather data path from crate_info since it is inherited from real crate for rust_doc and rust_test
    # Deduplicate data paths due to https://github.com/bazelbuild/bazel/issues/14681
    data_paths = depset(direct = getattr(attr, "data", []), transitive = [crate_info.compile_data_targets]).to_list()

    add_edition_flags(rustc_flags, crate_info)
    _add_lto_flags(ctx, toolchain, rustc_flags, crate_info)
    _add_codegen_units_flags(toolchain, emit, rustc_flags)

    # Use linker_type to determine whether to use direct or indirect linker invocation
    # If linker_type is not explicitly set, infer from which linker is actually being used
    ld_is_direct_driver = False

    # Link!
    if ("link" in emit and crate_info.type not in ["rlib", "lib"]) or add_flags_for_binary:
        # Rust's built-in linker can handle linking wasm files. We don't want to attempt to use the cc
        # linker since it won't understand.
        compilation_mode = ctx.var["COMPILATION_MODE"]
        if toolchain.target_arch not in ("wasm32", "wasm64"):
            if output_dir:
                use_pic = _should_use_pic(cc_toolchain, feature_configuration, crate_info.type, compilation_mode)
                rpaths = _compute_rpaths(toolchain, output_dir, dep_info, use_pic)
            else:
                rpaths = depset()

            ld, ld_is_direct_driver, link_args, link_env = get_linker_and_args(
                ctx,
                crate_info.type,
                toolchain,
                cc_toolchain,
                feature_configuration,
                rpaths,
                add_flags_for_binary = add_flags_for_binary,
            )

            env.update(link_env)
            rustc_flags.add(ld, format = "--codegen=linker=%s")

            # Split link args into individual "--codegen=link-arg=" flags to handle nested spaces.
            # Additional context: https://github.com/rust-lang/rust/pull/36574
            rustc_flags.add_all(link_args, format_each = "--codegen=link-arg=%s")

        _add_native_link_flags(
            rustc_flags,
            dep_info,
            linkstamp_outs,
            ambiguous_libs,
            crate_info.type,
            toolchain,
            cc_toolchain,
            feature_configuration,
            compilation_mode,
            ld_is_direct_driver,
            include_link_flags = include_link_flags,
        )

    use_metadata = _depend_on_metadata(crate_info, force_depend_on_objects)

    # These always need to be added, even if not linking this crate.
    add_crate_link_flags(rustc_flags, dep_info, force_all_deps_direct, use_metadata)

    needs_extern_proc_macro_flag = _is_proc_macro(crate_info) and crate_info.edition != "2015"
    if needs_extern_proc_macro_flag:
        rustc_flags.add("--extern")
        rustc_flags.add("proc_macro")

    if toolchain.llvm_cov and ctx.configuration.coverage_enabled:
        # https://doc.rust-lang.org/rustc/instrument-coverage.html
        pass

    if toolchain._experimental_link_std_dylib:
        rustc_flags.add("--codegen=prefer-dynamic")

    # Make bin crate data deps available to tests.
    for data in getattr(attr, "data", []):
        if rust_common.crate_info in data:
            dep_crate_info = data[rust_common.crate_info]
            if dep_crate_info.type == "bin":
                # Trying to make CARGO_BIN_EXE_{} canonical across platform by strip out extension if exists
                env_basename = dep_crate_info.output.basename[:-(1 + len(dep_crate_info.output.extension))] if len(dep_crate_info.output.extension) > 0 else dep_crate_info.output.basename
                env["CARGO_BIN_EXE_" + env_basename] = dep_crate_info.output.short_path

    # Add environment variables from the Rust toolchain.
    env.update(toolchain.env)

    # Update environment with user provided variables.
    if skip_expanding_rustc_env:
        env.update(crate_info.rustc_env)
    else:
        env.update(expand_dict_value_locations(
            ctx,
            crate_info.rustc_env,
            data_paths,
            {},
        ))

    # Ensure the sysroot is set for the target platform
    if toolchain._toolchain_generated_sysroot:
        rustc_flags.add(toolchain.sysroot, format = "--sysroot=%s")

    if toolchain._rename_first_party_crates:
        env["RULES_RUST_THIRD_PARTY_DIR"] = toolchain._third_party_dir

    # extra_rustc_env applies to the target configuration, not the exec configuration.
    if hasattr(ctx.attr, "_extra_rustc_env") and not is_exec_configuration(ctx):
        env.update(ctx.attr._extra_rustc_env[ExtraRustcEnvInfo].extra_rustc_env)

    if hasattr(ctx.attr, "_extra_exec_rustc_env") and is_exec_configuration(ctx):
        env.update(ctx.attr._extra_exec_rustc_env[ExtraExecRustcEnvInfo].extra_exec_rustc_env)

    rustc_flags.add_all(collect_extra_rustc_flags(ctx, toolchain, crate_info.root, crate_info.type), map_each = map_flag)

    if is_no_std(ctx, toolchain, crate_info.is_test):
        rustc_flags.add('--cfg=feature="no_std"')

    # Add target specific flags last, so they can override previous flags
    rustc_flags.add_all(
        expand_list_element_locations(
            ctx,
            getattr(attr, "rustc_flags", []),
            data_paths,
            {},
        ),
        map_each = map_flag,
    )

    # Needed for bzlmod-aware runfiles resolution.
    env["REPOSITORY_NAME"] = ctx.label.workspace_name

    # Create a struct which keeps the arguments separate so each may be tuned or
    # replaced where necessary
    args = struct(
        process_wrapper_flags = process_wrapper_flags,
        rustc_path = rustc_path,
        rustc_flags = rustc_flags,
        all = [process_wrapper_flags, rustc_path, rustc_flags],
    )

    return args, env

def collect_extra_rustc_flags(ctx, toolchain, crate_root, crate_type):
    """Gather all 'extra' rustc flags from the target's attributes and toolchain.

    Args:
        ctx (ctx): The current rule's context object.
        toolchain (rust_toolchain): The current Rust toolchain.
        crate_root (File): The root file of the crate.
        crate_type (str): The crate type.

    Returns:
        List[str]: Extra rustc flags.
    """
    flags = []

    if crate_type in toolchain.extra_rustc_flags_for_crate_types.keys():
        flags.extend(toolchain.extra_rustc_flags_for_crate_types[crate_type])

    is_exec = is_exec_configuration(ctx)

    flags.extend(toolchain.extra_exec_rustc_flags if is_exec else toolchain.extra_rustc_flags)

    if hasattr(ctx.attr, "_extra_rustc_flags") and not is_exec:
        flags.extend(ctx.attr._extra_rustc_flags[ExtraRustcFlagsInfo].extra_rustc_flags)

    if hasattr(ctx.attr, "_extra_rustc_flag") and not is_exec:
        flags.extend(ctx.attr._extra_rustc_flag[ExtraRustcFlagsInfo].extra_rustc_flags)

    if hasattr(ctx.attr, "_per_crate_rustc_flag") and not is_exec:
        per_crate_rustc_flags = ctx.attr._per_crate_rustc_flag[PerCrateRustcFlagsInfo].per_crate_rustc_flags
        flags.extend(_collect_per_crate_rustc_flags(ctx, crate_root, per_crate_rustc_flags))

    if hasattr(ctx.attr, "_extra_exec_rustc_flags") and is_exec:
        flags.extend(ctx.attr._extra_exec_rustc_flags[ExtraExecRustcFlagsInfo].extra_exec_rustc_flags)

    if hasattr(ctx.attr, "_extra_exec_rustc_flag") and is_exec:
        flags.extend(ctx.attr._extra_exec_rustc_flag[ExtraExecRustcFlagsInfo].extra_exec_rustc_flags)

    return flags

def rustc_compile_action(
        *,
        ctx,
        attr,
        toolchain,
        rust_flags = [],
        output_hash = None,
        force_all_deps_direct = False,
        crate_info_dict = None,
        skip_expanding_rustc_env = False,
        include_coverage = True):
    """Create and run a rustc compile action based on the current rule's attributes

    Args:
        ctx (ctx): The rule's context object
        attr (struct): Attributes to use for the rust compile action
        toolchain (rust_toolchain): The current `rust_toolchain`
        output_hash (str, optional): The hashed path of the crate root. Defaults to None.
        rust_flags (list, optional): Additional flags to pass to rustc. Defaults to [].
        force_all_deps_direct (bool, optional): Whether to pass the transitive rlibs with --extern
            to the commandline as opposed to -L.
        crate_info_dict: A mutable dict used to create CrateInfo provider
        skip_expanding_rustc_env (bool, optional): Whether to expand CrateInfo.rustc_env
        include_coverage (bool, optional): Whether to generate coverage information or not.

    Returns:
        list: A list of the following providers:
            - (CrateInfo): info for the crate we just built; same as `crate_info` parameter.
            - (DepInfo): The transitive dependencies of this crate.
            - (DefaultInfo): The output file for this crate, and its runfiles.
    """
    deps = crate_info_dict.pop("deps")
    proc_macro_deps = crate_info_dict.pop("proc_macro_deps")
    srcs = crate_info_dict.pop("srcs")

    crate_info = rust_common.create_crate_info(
        deps = depset(deps),
        proc_macro_deps = depset(proc_macro_deps),
        srcs = depset(srcs),
        **crate_info_dict
    )

    build_metadata = crate_info.metadata
    rustc_output = crate_info.rustc_output
    rustc_rmeta_output = crate_info.rustc_rmeta_output

    # Determine whether to use cc_common.link:
    #  * either if experimental_use_cc_common_link is 1,
    #  * or if experimental_use_cc_common_link is -1 and
    #    the toolchain experimental_use_cc_common_link is true.
    experimental_use_cc_common_link = False
    if hasattr(ctx.attr, "experimental_use_cc_common_link"):
        if ctx.attr.experimental_use_cc_common_link == 0:
            experimental_use_cc_common_link = False
        elif ctx.attr.experimental_use_cc_common_link == 1:
            experimental_use_cc_common_link = True
        elif ctx.attr.experimental_use_cc_common_link == -1:
            experimental_use_cc_common_link = toolchain._experimental_use_cc_common_link

    dep_info, build_info, linkstamps = collect_deps(
        deps = deps,
        proc_macro_deps = proc_macro_deps,
        aliases = crate_info.aliases,
    )
    extra_disabled_features = [RUST_LINK_CC_FEATURE]
    if crate_info.type in ["bin", "cdylib"] and dep_info.transitive_noncrates.to_list():
        # One or more of the transitive deps is a cc_library / cc_import
        extra_disabled_features = []
    cc_toolchain, feature_configuration = find_cc_toolchain(ctx, extra_disabled_features)
    if not cc_toolchain or not _are_linkstamps_supported(feature_configuration = feature_configuration):
        linkstamps = depset([])

    # Determine if the build is currently running with --stamp
    stamp = is_stamping_enabled(ctx, attr)

    # Add flags for any 'rustc' lints that are specified.
    #
    # Exclude lints if we're building in the exec configuration to prevent crates
    # used in build scripts from generating warnings.
    lint_files = []
    if hasattr(ctx.attr, "lint_config") and ctx.attr.lint_config and not is_exec_configuration(ctx):
        rust_flags = rust_flags + ctx.attr.lint_config[LintsInfo].rustc_lint_flags
        lint_files = lint_files + ctx.attr.lint_config[LintsInfo].rustc_lint_files

    compile_inputs, out_dir, build_env_files, build_flags_files, linkstamp_outs, ambiguous_libs = collect_inputs(
        ctx = ctx,
        file = ctx.file,
        files = ctx.files,
        linkstamps = linkstamps,
        toolchain = toolchain,
        cc_toolchain = cc_toolchain,
        feature_configuration = feature_configuration,
        crate_info = crate_info,
        dep_info = dep_info,
        build_info = build_info,
        lint_files = lint_files,
        stamp = stamp,
        experimental_use_cc_common_link = experimental_use_cc_common_link,
    )

    # The types of rustc outputs to emit.
    # If we build metadata, we need to keep the command line of the two invocations
    # (rlib and rmeta) as similar as possible, otherwise rustc rejects the rmeta as
    # a candidate.
    # Because of that we need to add emit=metadata to both the rlib and rmeta invocation.
    #
    # When cc_common linking is enabled, emit a `.o` file, which is later
    # passed to the cc_common.link action.
    emit = ["dep-info", "link"]
    if build_metadata:
        emit.append("metadata")
    if experimental_use_cc_common_link:
        emit = ["obj"]

    # Determine whether to pass `--require-explicit-unstable-features true` to the process wrapper:
    require_explicit_unstable_features = False
    if hasattr(ctx.attr, "require_explicit_unstable_features"):
        if ctx.attr.require_explicit_unstable_features == 0:
            require_explicit_unstable_features = False
        elif ctx.attr.require_explicit_unstable_features == 1:
            require_explicit_unstable_features = True
        elif ctx.attr.require_explicit_unstable_features == -1:
            require_explicit_unstable_features = toolchain.require_explicit_unstable_features

    args, env_from_args = construct_arguments(
        ctx = ctx,
        attr = attr,
        file = ctx.file,
        toolchain = toolchain,
        tool_path = toolchain.rustc.path,
        cc_toolchain = cc_toolchain,
        emit = emit,
        feature_configuration = feature_configuration,
        crate_info = crate_info,
        dep_info = dep_info,
        linkstamp_outs = linkstamp_outs,
        ambiguous_libs = ambiguous_libs,
        output_hash = output_hash,
        rust_flags = rust_flags,
        out_dir = out_dir,
        build_env_files = build_env_files,
        build_flags_files = build_flags_files,
        force_all_deps_direct = force_all_deps_direct,
        stamp = stamp,
        use_json_output = bool(build_metadata) or bool(rustc_output) or bool(rustc_rmeta_output),
        skip_expanding_rustc_env = skip_expanding_rustc_env,
        require_explicit_unstable_features = require_explicit_unstable_features,
    )

    args_metadata = None
    if build_metadata:
        args_metadata, _ = construct_arguments(
            ctx = ctx,
            attr = attr,
            file = ctx.file,
            toolchain = toolchain,
            tool_path = toolchain.rustc.path,
            cc_toolchain = cc_toolchain,
            emit = emit,
            feature_configuration = feature_configuration,
            crate_info = crate_info,
            dep_info = dep_info,
            linkstamp_outs = linkstamp_outs,
            ambiguous_libs = ambiguous_libs,
            output_hash = output_hash,
            rust_flags = rust_flags,
            out_dir = out_dir,
            build_env_files = build_env_files,
            build_flags_files = build_flags_files,
            force_all_deps_direct = force_all_deps_direct,
            stamp = stamp,
            use_json_output = True,
            build_metadata = True,
            require_explicit_unstable_features = require_explicit_unstable_features,
        )

    env = dict(ctx.configuration.default_shell_env)

    # this is the final list of env vars
    env.update(env_from_args)

    if hasattr(attr, "version") and attr.version != "0.0.0":
        formatted_version = " v{}".format(attr.version)
    else:
        formatted_version = ""

    # Declares the outputs of the rustc compile action.
    # By default this is the binary output; if cc_common.link is used, this is
    # the main `.o` file (`output_o` below).
    outputs = [crate_info.output]

    # The `.o` output file, only used for linking via cc_common.link.
    output_o = None
    if experimental_use_cc_common_link:
        obj_ext = ".o"
        output_o = ctx.actions.declare_file(crate_info.name + obj_ext, sibling = crate_info.output)
        outputs = [output_o]

    # For a cdylib that might be added as a dependency to a cc_* target on Windows, it is important to include the
    # interface library that rustc generates in the output files.
    interface_library = None
    if toolchain.target_os == "windows" and crate_info.type == "cdylib":
        # Rustc generates the import library with a `.dll.lib` extension rather than the usual `.lib` one that msvc
        # expects (see https://github.com/rust-lang/rust/pull/29520 for more context).
        interface_library = ctx.actions.declare_file(crate_info.output.basename + ".lib", sibling = crate_info.output)
        outputs.append(interface_library)

    # The action might generate extra output that we don't want to include in the `DefaultInfo` files.
    action_outputs = list(outputs)
    if rustc_output:
        action_outputs.append(rustc_output)

    # Get the compilation mode for the current target.
    compilation_mode = get_compilation_mode_opts(ctx, toolchain)

    # Rustc generates a pdb file (on Windows) or a dsym folder (on macos) so provide it in an output group for crate
    # types that benefit from having debug information in a separate file.
    pdb_file = None
    dsym_folder = None
    if crate_info.type in ("cdylib", "bin") and not experimental_use_cc_common_link:
        if toolchain.target_os == "windows" and compilation_mode.strip_level == "none":
            pdb_file = ctx.actions.declare_file(crate_info.output.basename[:-len(crate_info.output.extension)] + "pdb", sibling = crate_info.output)
            action_outputs.append(pdb_file)
        elif toolchain.target_os in ["macos", "darwin"]:
            dsym_folder = ctx.actions.declare_directory(crate_info.output.basename + ".dSYM", sibling = crate_info.output)
            action_outputs.append(dsym_folder)

    if ctx.executable._process_wrapper:
        # Run as normal
        ctx.actions.run(
            executable = ctx.executable._process_wrapper,
            inputs = compile_inputs,
            outputs = action_outputs,
            env = env,
            arguments = args.all,
            mnemonic = "Rustc",
            progress_message = "Compiling Rust {} {}{} ({} file{})".format(
                crate_info.type,
                ctx.label.name,
                formatted_version,
                len(srcs),
                "" if len(srcs) == 1 else "s",
            ),
            toolchain = "@rules_rust//rust:toolchain_type",
            resource_set = get_rustc_resource_set(toolchain),
        )
        if args_metadata:
            ctx.actions.run(
                executable = ctx.executable._process_wrapper,
                inputs = compile_inputs,
                outputs = [build_metadata] + [x for x in [rustc_rmeta_output] if x],
                env = env,
                arguments = args_metadata.all,
                mnemonic = "RustcMetadata",
                progress_message = "Compiling Rust metadata {} {}{} ({} file{})".format(
                    crate_info.type,
                    ctx.label.name,
                    formatted_version,
                    len(srcs),
                    "" if len(srcs) == 1 else "s",
                ),
                toolchain = "@rules_rust//rust:toolchain_type",
            )
    elif hasattr(ctx.executable, "_bootstrap_process_wrapper"):
        # Run without process_wrapper
        if build_env_files or build_flags_files or stamp or build_metadata:
            fail("build_env_files, build_flags_files, stamp, build_metadata are not supported when building without process_wrapper")
        ctx.actions.run(
            executable = ctx.executable._bootstrap_process_wrapper,
            inputs = compile_inputs,
            outputs = action_outputs,
            env = env,
            arguments = [args.rustc_path, args.rustc_flags],
            mnemonic = "Rustc",
            progress_message = "Compiling Rust (without process_wrapper) {} {}{} ({} file{})".format(
                crate_info.type,
                ctx.label.name,
                formatted_version,
                len(srcs),
                "" if len(srcs) == 1 else "s",
            ),
            toolchain = "@rules_rust//rust:toolchain_type",
            resource_set = get_rustc_resource_set(toolchain),
        )
    else:
        fail("No process wrapper was defined for {}".format(ctx.label))

    if experimental_use_cc_common_link:
        # Wrap the main `.o` file into a compilation output suitable for
        # cc_common.link. The main `.o` file is useful in both PIC and non-PIC
        # modes.
        compilation_outputs = cc_common.create_compilation_outputs(
            objects = depset([output_o]),
            pic_objects = depset([output_o]),
        )

        malloc_library = ctx.attr._custom_malloc or ctx.attr.malloc

        # Collect the linking contexts of the standard library and dependencies.
        linking_contexts = [
            malloc_library[CcInfo].linking_context,
            _get_std_and_alloc_info(ctx, toolchain, crate_info).linking_context,
            toolchain.stdlib_linkflags.linking_context,
        ]

        for dep in crate_info.deps.to_list():
            if dep.cc_info:
                linking_contexts.append(dep.cc_info.linking_context)

        # In the cc_common.link action we need to pass the name of the final
        # binary (output) relative to the package of this target.
        # We compute it by stripping the path to the package directory,
        # which is a prefix of the path of `crate_info.output`.

        # The path to the package dir, including a trailing "/".
        package_dir = ctx.bin_dir.path + "/"

        # For external repositories, workspace root is not part of the output
        # path when sibling repository layout is used (the repository name is
        # part of the bin_dir). This scenario happens when the workspace root
        # starts with "../"
        if ctx.label.workspace_root and not ctx.label.workspace_root.startswith("../"):
            package_dir = package_dir + ctx.label.workspace_root + "/"
        if ctx.label.package:
            package_dir = package_dir + ctx.label.package + "/"

        if not crate_info.output.path.startswith(package_dir):
            fail("The package dir path", package_dir, "should be a prefix of the crate_info.output.path", crate_info.output.path)

        output_relative_to_package = crate_info.output.path[len(package_dir):]

        # Compile actions that produce shared libraries create output of the form "libfoo.so" for linux and macos;
        # cc_common.link expects us to pass "foo" to the name parameter. We cannot simply use crate_info.name because
        # the name of the crate does not always match the name of output file, e.g a crate named foo-bar will produce
        # a (lib)foo_bar output file.
        if crate_info.type == "cdylib":
            output_lib = crate_info.output.basename
            if toolchain.target_os != "windows":
                # Strip the leading "lib" prefix
                output_lib = output_lib[3:]

            # Strip the file extension
            output_lib = output_lib[:-(1 + len(crate_info.output.extension))]

            # Remove the basename (which contains the undesired 'lib' prefix and the file extension)
            output_relative_to_package = output_relative_to_package[:-len(crate_info.output.basename)]

            # Append the name of the library
            output_relative_to_package = output_relative_to_package + output_lib

        additional_linker_outputs = []
        if crate_info.type in ("cdylib", "bin") and cc_common.is_enabled(feature_configuration = feature_configuration, feature_name = "generate_pdb_file"):
            pdb_file = ctx.actions.declare_file(crate_info.output.basename[:-len(crate_info.output.extension)] + "pdb", sibling = crate_info.output)
            additional_linker_outputs.append(pdb_file)

        cc_common.link(
            actions = ctx.actions,
            feature_configuration = feature_configuration,
            cc_toolchain = cc_toolchain,
            linking_contexts = linking_contexts,
            compilation_outputs = compilation_outputs,
            name = output_relative_to_package,
            stamp = ctx.attr.stamp,
            output_type = "executable" if crate_info.type == "bin" else "dynamic_library",
            additional_outputs = additional_linker_outputs,
        )

        outputs = [crate_info.output]

    coverage_runfiles = []
    if toolchain.llvm_cov and ctx.configuration.coverage_enabled and crate_info.is_test:
        coverage_runfiles = [toolchain.llvm_cov, toolchain.llvm_profdata] + toolchain.llvm_lib

    experimental_use_coverage_metadata_files = toolchain._experimental_use_coverage_metadata_files

    runfiles = ctx.runfiles(
        files = getattr(ctx.files, "data", []) +
                ([] if experimental_use_coverage_metadata_files else coverage_runfiles),
    )
    transitive_runfiles = []
    crate_attr = getattr(ctx.attr, "crate", None)
    for runfiles_attr in (
        getattr(ctx.attr, "srcs", []),
        getattr(ctx.attr, "deps", []),
        getattr(ctx.attr, "data", []),
        [crate_attr] if crate_attr else [],
    ):
        if not runfiles_attr:
            continue
        for target in runfiles_attr:
            transitive_runfiles.append(target[DefaultInfo].default_runfiles)
    if crate_info.type in ["bin", "cdylib", "staticlib"]:
        dynamic_libraries = ctx.runfiles(files = [
            library_to_link.dynamic_library
            for dep in getattr(ctx.attr, "deps", [])
            if CcInfo in dep
            for linker_input in dep[CcInfo].linking_context.linker_inputs.to_list()
            for library_to_link in linker_input.libraries
            if _is_dylib(library_to_link) and library_to_link.dynamic_library
        ])
        transitive_runfiles.append(dynamic_libraries)
    runfiles = runfiles.merge_all(transitive_runfiles)

    # TODO: Remove after some resolution to
    # https://github.com/bazelbuild/rules_rust/issues/771
    out_binary = getattr(attr, "out_binary", False)

    executable = crate_info.output if crate_info.type == "bin" or crate_info.is_test or out_binary else None

    instrumented_files_kwargs = {
        "dependency_attributes": ["deps", "crate"],
        "extensions": ["rs"],
        "source_attributes": ["srcs"],
    }

    if experimental_use_coverage_metadata_files:
        instrumented_files_kwargs.update({
            "metadata_files": coverage_runfiles + [executable] if executable else [],
        })

    providers = [
        DefaultInfo(
            # nb. This field is required for cc_library to depend on our output.
            files = depset(outputs),
            runfiles = runfiles,
            executable = executable,
        ),
    ]

    # When invoked by aspects (and when running `bazel coverage`), the
    # baseline_coverage.dat created here will conflict with the baseline_coverage.dat of the
    # underlying target, which is a build failure. So we add an option to disable it so that this
    # function can be invoked from aspects for rules that have its own InstrumentedFilesInfo.
    if include_coverage:
        providers.append(
            coverage_common.instrumented_files_info(
                ctx,
                **instrumented_files_kwargs
            ),
        )

    if crate_info_dict != None:
        crate_info_dict.update({
            "rustc_env": env,
        })
        crate_info = rust_common.create_crate_info(
            deps = depset(deps),
            proc_macro_deps = depset(proc_macro_deps),
            srcs = depset(srcs),
            **crate_info_dict
        )

    if crate_info.type in ["staticlib", "cdylib"] and not out_binary:
        # These rules are not supposed to be depended on by other rust targets, and
        # as such they shouldn't provide a CrateInfo. However, one may still want to
        # write a rust_test for them, so we provide the CrateInfo wrapped in a provider
        # that rust_test understands.
        providers.extend([rust_common.test_crate_info(crate = crate_info), dep_info])
    else:
        providers.extend([crate_info, dep_info])

    providers += establish_cc_info(ctx, attr, crate_info, toolchain, cc_toolchain, feature_configuration, interface_library)

    output_group_info = {}

    if pdb_file:
        output_group_info["pdb_file"] = depset([pdb_file])
    if dsym_folder:
        output_group_info["dsym_folder"] = depset([dsym_folder])
    if build_metadata:
        output_group_info["build_metadata"] = depset([build_metadata])
        if rustc_rmeta_output:
            output_group_info["rustc_rmeta_output"] = depset([rustc_rmeta_output])
    if rustc_output:
        output_group_info["rustc_output"] = depset([rustc_output])

    if output_group_info:
        providers.append(OutputGroupInfo(**output_group_info))

    # A bit unfortunate, but sidecar the lints info so rustdoc can access the
    # set of lints from the target it is documenting.
    if hasattr(ctx.attr, "lint_config") and ctx.attr.lint_config:
        providers.append(ctx.attr.lint_config[LintsInfo])

    return providers

def is_no_std(ctx, toolchain, crate_is_test):
    return not (is_exec_configuration(ctx) or crate_is_test or toolchain._no_std == "off")

def _should_use_rustc_allocator_libraries(toolchain):
    use_or_default = toolchain._experimental_use_allocator_libraries_with_mangled_symbols
    if use_or_default not in [-1, 0, 1]:
        fail("unexpected value of experimental_use_allocator_libraries_with_mangled_symbols (should be one of [-1, 0, 1]): " + use_or_default)
    if use_or_default == -1:
        return toolchain._experimental_use_allocator_libraries_with_mangled_symbols_setting
    return bool(use_or_default)

def _get_std_and_alloc_info(ctx, toolchain, crate_info):
    # Handles standard libraries and allocator shims.
    #
    # The standard libraries vary between "std" and "nostd" flavors.
    #
    # The allocator libraries vary along two dimensions:
    # * the type of rust allocator used (default or global)
    # * the mechanism providing the libraries (via the rust rules
    #   allocator_libraries attribute, or via the rust toolchain allocator
    #   attributes).
    #
    # When provided, the allocator_libraries attribute takes precedence over the
    # toolchain allocator attributes.
    libs = None
    attr_allocator_library = None
    attr_global_allocator_library = None
    if _should_use_rustc_allocator_libraries(toolchain) and hasattr(ctx.attr, "allocator_libraries"):
        libs = ctx.attr.allocator_libraries[AllocatorLibrariesInfo]
        attr_allocator_library = libs.allocator_library
        attr_global_allocator_library = libs.global_allocator_library
    if is_exec_configuration(ctx):
        if attr_allocator_library:
            return libs.libstd_and_allocator_ccinfo
        return toolchain.libstd_and_allocator_ccinfo
    if toolchain._experimental_use_global_allocator:
        if is_no_std(ctx, toolchain, crate_info.is_test):
            if attr_global_allocator_library:
                return libs.nostd_and_global_allocator_ccinfo
            return toolchain.nostd_and_global_allocator_ccinfo
        else:
            if attr_global_allocator_library:
                return libs.libstd_and_global_allocator_ccinfo
            return toolchain.libstd_and_global_allocator_ccinfo
    if attr_allocator_library:
        return libs.libstd_and_allocator_ccinfo
    return toolchain.libstd_and_allocator_ccinfo

def _is_dylib(dep):
    """Determine if the dependency represents a dynamic library.

    Args:
        dep (LibraryToLink): The `LibraryToLink` component of a target (e.g. from `CcInfo`)

    Returns:
        bool: True if the dependency should be thought of as a dynamic library.
    """
    return not bool(dep.static_library or dep.pic_static_library)

def _collect_nonstatic_linker_inputs(cc_info):
    shared_linker_inputs = []
    for linker_input in cc_info.linking_context.linker_inputs.to_list():
        dylibs = [
            lib
            for lib in linker_input.libraries
            if _is_dylib(lib)
        ]
        if dylibs:
            shared_linker_inputs.append(cc_common.create_linker_input(
                owner = linker_input.owner,
                libraries = depset(dylibs),
            ))
    return shared_linker_inputs

def _add_lto_flags(ctx, toolchain, args, crate):
    """Adds flags to an Args object to configure LTO for 'rustc'.

    Args:
        ctx (ctx): The calling rule's context object.
        toolchain (rust_toolchain): The current target's `rust_toolchain`.
        args (Args): A reference to an Args object
        crate (CrateInfo): A CrateInfo provider
    """
    lto_args = construct_lto_arguments(ctx, toolchain, crate)
    args.add_all(lto_args)

def _add_codegen_units_flags(toolchain, emit, args):
    """Adds flags to an Args object to configure codegen_units for 'rustc'.

    https://doc.rust-lang.org/rustc/codegen-options/index.html#codegen-units

    Args:
        toolchain (rust_toolchain): The current target's `rust_toolchain`.
        emit (list): Values for the --emit flag to rustc.
        args (Args): A reference to an Args object
    """

    # If we're emitting an object file, the build rules expect to see a single
    # object file.
    if _will_emit_object_file(emit):
        args.add("-Ccodegen-units=1")
        return

    if not is_codegen_units_enabled(toolchain):
        return

    args.add("-Ccodegen-units={}".format(toolchain._codegen_units))

def establish_cc_info(ctx, attr, crate_info, toolchain, cc_toolchain, feature_configuration, interface_library):
    """If the produced crate is suitable yield a CcInfo to allow for interop with cc rules

    Args:
        ctx (ctx): The rule's context object
        attr (struct): Attributes to use in gathering CcInfo
        crate_info (CrateInfo): The CrateInfo provider of the target crate
        toolchain (rust_toolchain): The current `rust_toolchain`
        cc_toolchain (CcToolchainInfo): The current `CcToolchainInfo`
        feature_configuration (FeatureConfiguration): Feature configuration to be queried.
        interface_library (File): Optional interface library for cdylib crates on Windows.

    Returns:
        list: A list containing the `CcInfo` provider and optionally `AllocatorLibrariesImplInfo`
            provider used when this crate is used as the rust allocator library implementation.
    """

    # A test will not need to produce CcInfo as nothing can depend on test targets
    if crate_info.is_test:
        return []

    # Only generate CcInfo for particular crate types
    if crate_info.type not in ("staticlib", "cdylib", "rlib", "lib"):
        return []

    # TODO: Remove after some resolution to
    # https://github.com/bazelbuild/rules_rust/issues/771
    if getattr(attr, "out_binary", False):
        return []

    dot_a = None
    library_to_link = None

    if crate_info.type == "staticlib":
        if cc_toolchain:
            library_to_link = cc_common.create_library_to_link(
                actions = ctx.actions,
                feature_configuration = feature_configuration,
                cc_toolchain = cc_toolchain,
                static_library = crate_info.output,
                # TODO(hlopko): handle PIC/NOPIC correctly
                pic_static_library = crate_info.output,
                alwayslink = getattr(attr, "alwayslink", False),
            )
    elif crate_info.type in ("rlib", "lib"):
        # bazel hard-codes a check for endswith((".a", ".pic.a",
        # ".lib")) in create_library_to_link, so we work around that
        # by creating a symlink to the .rlib with a .a extension.
        dot_a = make_static_lib_symlink(ctx.label.package, ctx.actions, crate_info.output)

        if cc_toolchain:
            # TODO(hlopko): handle PIC/NOPIC correctly
            library_to_link = cc_common.create_library_to_link(
                actions = ctx.actions,
                feature_configuration = feature_configuration,
                cc_toolchain = cc_toolchain,
                static_library = dot_a,
                # TODO(hlopko): handle PIC/NOPIC correctly
                pic_static_library = dot_a,
                alwayslink = getattr(attr, "alwayslink", False),
            )
    elif crate_info.type == "cdylib":
        if cc_toolchain:
            library_to_link = cc_common.create_library_to_link(
                actions = ctx.actions,
                feature_configuration = feature_configuration,
                cc_toolchain = cc_toolchain,
                dynamic_library = crate_info.output,
                interface_library = interface_library,
            )
    else:
        fail("Unexpected case")

    link_input = cc_common.create_linker_input(
        owner = ctx.label,
        libraries = depset([library_to_link]) if library_to_link else depset(),
    )

    linking_context = cc_common.create_linking_context(
        # TODO - What to do for no_std?
        linker_inputs = depset([link_input]),
    )

    cc_infos = [
        CcInfo(linking_context = linking_context),
        toolchain.stdlib_linkflags,
    ]

    # Flattening is okay since crate_info.deps only records direct deps.
    for dep in crate_info.deps.to_list():
        if dep.cc_info:
            # A Rust staticlib or shared library doesn't need to propagate linker inputs
            # of its dependencies, except for shared libraries.
            if crate_info.type in ["cdylib", "staticlib"]:
                shared_linker_inputs = _collect_nonstatic_linker_inputs(dep.cc_info)
                if shared_linker_inputs:
                    linking_context = cc_common.create_linking_context(
                        linker_inputs = depset(shared_linker_inputs),
                    )
                    cc_infos.append(CcInfo(linking_context = linking_context))
            else:
                cc_infos.append(dep.cc_info)

    if crate_info.type in ("rlib", "lib"):
        libstd_and_allocator_cc_info = _get_std_and_alloc_info(ctx, toolchain, crate_info)
        if libstd_and_allocator_cc_info:
            # TODO: if we already have an rlib in our deps, we could skip this
            cc_infos.append(libstd_and_allocator_cc_info)

    providers = [cc_common.merge_cc_infos(cc_infos = cc_infos)]
    if crate_info.type == "staticlib":
        # The static archive is the output.
        dot_a = crate_info.output
    if dot_a:
        providers.append(AllocatorLibrariesImplInfo(static_archive = dot_a))
    return providers

def add_edition_flags(args, crate):
    """Adds the Rust edition flag to an arguments object reference

    Args:
        args (Args): A reference to an Args object
        crate (CrateInfo): A CrateInfo provider
    """
    if crate.edition != "2015":
        args.add(crate.edition, format = "--edition=%s")

def _process_build_scripts(
        build_info,
        dep_info,
        include_link_flags = True):
    """Gathers the outputs from a target's `cargo_build_script` action.

    Args:
        build_info (BuildInfo): The target Build's dependency info.
        dep_info (DepInfo): The Depinfo provider form the target Crate's set of inputs.
        include_link_flags (bool, optional): Whether to include flags like `-l` that instruct the linker to search for a library.

    Returns:
        tuple: A tuple: A tuple of the following items:
            - (depset[File]): A list of all build info `OUT_DIR` File objects
            - (str): The `OUT_DIR` of the current build info
            - (File): An optional path to a generated environment file from a `cargo_build_script` target
            - (depset[File]): All direct and transitive build flags from the current build info.
    """
    direct_inputs = []
    transitive_inputs = [dep_info.link_search_path_files, dep_info.transitive_data]

    # Arguments to the commandline line wrapper that are going to be used
    # to create the final command line
    out_dir = None
    build_env_file = None
    build_flags_files = []

    # We include the direct dep build_info because crates which use cargo build scripts may need to e.g. include_str! a generated file.
    if build_info:
        if build_info.out_dir:
            out_dir = build_info.out_dir.path
            direct_inputs.append(build_info.out_dir)
        build_env_file = build_info.rustc_env
        if build_info.flags:
            build_flags_files.append(build_info.flags)
        if build_info.linker_flags and include_link_flags:
            build_flags_files.append(build_info.linker_flags)
            direct_inputs.append(build_info.linker_flags)

        transitive_inputs.append(build_info.compile_data)

    # We include transitive dep build_infos because cargo build scripts may generate files which get linked into the final binary.
    # This should probably only actually be exposed to actions which link.
    for dep_build_info in dep_info.transitive_build_infos.to_list():
        if dep_build_info.out_dir:
            direct_inputs.append(dep_build_info.out_dir)

    out_dir_compile_inputs = depset(
        direct_inputs,
        transitive = transitive_inputs,
    )

    return (
        out_dir_compile_inputs,
        out_dir,
        build_env_file,
        depset(build_flags_files, transitive = [dep_info.link_search_path_files]),
    )

def _compute_rpaths(toolchain, output_dir, dep_info, use_pic):
    """Determine the artifact's rpaths relative to the bazel root for runtime linking of shared libraries.

    Args:
        toolchain (rust_toolchain): The current `rust_toolchain`
        output_dir (str): The output directory of the current target
        dep_info (DepInfo): The current target's dependency info
        use_pic: If set, prefers pic_static_library over static_library.

    Returns:
        depset: A set of relative paths from the output directory to each dependency
    """

    # Windows has no rpath equivalent, so always return an empty depset.
    # Fuchsia assembles shared libraries during packaging.
    if toolchain.target_os == "windows" or toolchain.target_os == "fuchsia":
        return depset([])

    dylibs = [
        get_preferred_artifact(lib, use_pic)
        for linker_input in dep_info.transitive_noncrates.to_list()
        for lib in linker_input.libraries
        if _is_dylib(lib)
    ]

    # Include std dylib if dylib linkage is enabled
    if toolchain._experimental_link_std_dylib:
        # TODO: Make toolchain.rust_std to only include libstd.so
        # When dylib linkage is enabled, toolchain.rust_std should only need to
        # include libstd.so. Hence, no filtering needed.
        for file in toolchain.rust_std.to_list():
            if is_std_dylib(file):
                dylibs.append(file)

    if not dylibs:
        return depset([])

    # For darwin, dylibs compiled by Bazel will fail to be resolved at runtime
    # without a version of Bazel that includes
    # https://github.com/bazelbuild/bazel/pull/13427. This is known to not be
    # included in Bazel 4.1 and below.
    if toolchain.target_os not in ["linux", "darwin", "macos", "android"]:
        fail("Runtime linking is not supported on {}, but found {}".format(
            toolchain.target_os,
            dep_info.transitive_noncrates,
        ))

    # Multiple dylibs can be present in the same directory, so deduplicate them.
    return depset([
        relativize(lib_dir, output_dir)
        for lib_dir in _get_dir_names(dylibs)
    ])

def _get_dir_names(files):
    """Returns a list of directory names from the given list of File objects

    Args:
        files (list): A list of File objects

    Returns:
        list: A list of directory names for all files
    """
    dirs = {}
    for f in files:
        dirs[f.dirname] = None
    return dirs.keys()

def add_crate_link_flags(args, dep_info, force_all_deps_direct = False, use_metadata = False):
    """Adds link flags to an Args object reference

    Args:
        args (Args): An arguments object reference
        dep_info (DepInfo): The current target's dependency info
        force_all_deps_direct (bool, optional): Whether to pass the transitive rlibs with --extern
            to the commandline as opposed to -L.
        use_metadata (bool, optional): Build command line arguments using metadata for crates that provide it.
    """

    direct_crates = depset(
        transitive = [
            dep_info.direct_crates,
            dep_info.transitive_crates,
        ],
    ) if force_all_deps_direct else dep_info.direct_crates

    crate_to_link_flags = _crate_to_link_flag_metadata if use_metadata else _crate_to_link_flag
    args.add_all(direct_crates, uniquify = True, map_each = crate_to_link_flags)

    args.add_all(
        dep_info.transitive_crates,
        map_each = _get_crate_dirname,
        uniquify = True,
        format_each = "-Ldependency=%s",
    )

def _crate_to_link_flag_metadata(crate):
    """A helper macro used by `add_crate_link_flags` for adding crate link flags to a Arg object

    Args:
        crate (CrateInfo|AliasableDepInfo): A CrateInfo or an AliasableDepInfo provider

    Returns:
        list: Link flags for the given provider
    """

    # This is AliasableDepInfo, we should use the alias as a crate name
    if hasattr(crate, "dep"):
        name = crate.name
        crate_info = crate.dep
    else:
        name = crate.name
        crate_info = crate

    lib_or_meta = crate_info.metadata
    if not crate_info.metadata or not crate_info.metadata_supports_pipelining:
        lib_or_meta = crate_info.output
    return ["--extern={}={}".format(name, lib_or_meta.path)]

def _crate_to_link_flag(crate):
    """A helper macro used by `add_crate_link_flags` for adding crate link flags to a Arg object

    Args:
        crate (CrateInfo|AliasableDepInfo): A CrateInfo or an AliasableDepInfo provider

    Returns:
        list: Link flags for the given provider
    """

    # This is AliasableDepInfo, we should use the alias as a crate name
    if hasattr(crate, "dep"):
        name = crate.name
        crate_info = crate.dep
    else:
        name = crate.name
        crate_info = crate
    return ["--extern={}={}".format(name, crate_info.output.path)]

def _get_crate_dirname(crate):
    """A helper macro used by `add_crate_link_flags` for getting the directory name of the current crate's output path

    Args:
        crate (CrateInfo): A CrateInfo provider from the current rule

    Returns:
        str: The directory name of the the output File that will be produced.
    """
    return crate.output.dirname

def _portable_link_flags(lib, use_pic, ambiguous_libs, get_lib_name, for_windows = False, for_darwin = False, flavor_msvc = False):
    artifact = get_preferred_artifact(lib, use_pic)
    if ambiguous_libs and artifact.path in ambiguous_libs:
        artifact = ambiguous_libs[artifact.path]
    if lib.static_library or lib.pic_static_library:
        # To ensure appropriate linker library argument order, in the presence
        # of both native libraries that depend on rlibs and rlibs that depend
        # on native libraries, we use an approach where we "sandwich" the
        # rust libraries between two similar sections of all of native
        # libraries:
        # n1 n2 ... r1 r2 ... n1 n2 ...
        # A         B         C
        # This way any dependency from a native library to a rust library
        # is resolved from A to B, and any dependency from a rust library to
        # a native one is resolved from B to C.
        # The question of resolving dependencies from a native library from A
        # to any rust library is addressed in a different place, where we
        # create symlinks to the rlibs, pretending they are native libraries,
        # and adding references to these symlinks in the native section A.
        # We rely in the behavior of -Clink-arg to put the linker args
        # at the end of the linker invocation constructed by rustc.

        # We skip adding `-Clink-arg=-l` for libstd and libtest from the standard library, as
        # these two libraries are present both as an `.rlib` and a `.so` format.
        # On linux, Rustc adds a -Bdynamic to the linker command line before the libraries specified
        # with `-Clink-arg`, which leads to us linking against the `.so`s but not putting the
        # corresponding value to the runtime library search paths, which results in a
        # "cannot open shared object file: No such file or directory" error at execution time.
        # We can fix this by adding a `-Clink-arg=-Bstatic` on linux, but we don't have that option for
        # macos. The proper solution for this issue would be to remove `libtest-{hash}.so` and `libstd-{hash}.so`
        # from the toolchain. However, it is not enough to change the toolchain's `rust_std_{...}` filegroups
        # here: https://github.com/bazelbuild/rules_rust/blob/a9d5d894ad801002d007b858efd154e503796b9f/rust/private/repository_utils.bzl#L144
        # because rustc manages to escape the sandbox and still finds them at linking time.
        # We need to modify the repository rules to erase those files completely.
        if "lib/rustlib" in artifact.path and (
            artifact.basename.startswith("libtest-") or artifact.basename.startswith("libstd-") or
            artifact.basename.startswith("test-") or artifact.basename.startswith("std-")
        ):
            return [] if for_darwin else ["-lstatic=%s" % get_lib_name(artifact)]

        if for_windows:
            if flavor_msvc:
                return [
                    "-lstatic=%s" % get_lib_name(artifact),
                    "-Clink-arg={}".format(artifact.basename),
                ]
            else:
                return [
                    "-lstatic=%s" % get_lib_name(artifact),
                    "-Clink-arg=-l{}".format(artifact.basename),
                ]
        else:
            return [
                "-lstatic=%s" % get_lib_name(artifact),
                "-Clink-arg=-l{}".format(get_lib_name(artifact)),
            ]
    elif _is_dylib(lib):
        return [
            "-ldylib=%s" % get_lib_name(artifact),
        ]

    return []

def _add_user_link_flags(ret, linker_input):
    ret.extend(["--codegen=link-arg={}".format(flag) for flag in linker_input.user_link_flags])

def _make_link_flags_windows(make_link_flags_args, flavor_msvc, use_direct_driver):
    linker_input, use_pic, ambiguous_libs, include_link_flags = make_link_flags_args
    ret = []
    prefix = "" if use_direct_driver else "-Wl,"
    for lib in linker_input.libraries:
        if lib.alwayslink:
            if flavor_msvc:
                ret.append("-Clink-arg=/WHOLEARCHIVE:%s" % get_preferred_artifact(lib, use_pic).path)
            else:
                ret.extend([
                    ("-Clink-arg=%s--whole-archive" % prefix),
                    ("-Clink-arg=%s" % get_preferred_artifact(lib, use_pic).path),
                    ("-Clink-arg=%s--no-whole-archive" % prefix),
                ])
        elif include_link_flags:
            ret.extend(_portable_link_flags(lib, use_pic, ambiguous_libs, get_lib_name_for_windows, for_windows = True, flavor_msvc = flavor_msvc))
    _add_user_link_flags(ret, linker_input)
    return ret

def _make_link_flags_windows_msvc(make_link_flags_args, use_direct_driver):
    return _make_link_flags_windows(make_link_flags_args, flavor_msvc = True, use_direct_driver = use_direct_driver)

def _make_link_flags_windows_gnu(make_link_flags_args, use_direct_driver):
    return _make_link_flags_windows(make_link_flags_args, flavor_msvc = False, use_direct_driver = use_direct_driver)

def _make_link_flags_darwin(make_link_flags_args, use_direct_driver):
    linker_input, use_pic, ambiguous_libs, include_link_flags = make_link_flags_args
    ret = []
    prefix = "" if use_direct_driver else "-Wl,"
    for lib in linker_input.libraries:
        if lib.alwayslink:
            ret.extend([
                ("-Clink-arg=%s-force_load" % (prefix)),
                ("-Clink-arg=%s%s" % (prefix, get_preferred_artifact(lib, use_pic).path)),
            ])
        elif include_link_flags:
            ret.extend(_portable_link_flags(lib, use_pic, ambiguous_libs, get_lib_name_default, for_darwin = True))
    _add_user_link_flags(ret, linker_input)
    return ret

def _make_link_flags_default(make_link_flags_args, use_direct_driver):
    linker_input, use_pic, ambiguous_libs, include_link_flags = make_link_flags_args
    ret = []
    prefix = "" if use_direct_driver else "-Wl,"
    for lib in linker_input.libraries:
        if lib.alwayslink:
            ret.extend([
                ("-Clink-arg=%s--whole-archive" % prefix),
                ("-Clink-arg=%s" % get_preferred_artifact(lib, use_pic).path),
                ("-Clink-arg=%s--no-whole-archive" % prefix),
            ])
        elif include_link_flags:
            ret.extend(_portable_link_flags(lib, use_pic, ambiguous_libs, get_lib_name_default))
    _add_user_link_flags(ret, linker_input)
    return ret

def _make_link_flags_default_indirect(make_link_flags_args):
    return _make_link_flags_default(make_link_flags_args, False)

def _make_link_flags_default_direct(make_link_flags_args):
    return _make_link_flags_default(make_link_flags_args, True)

def _make_link_flags_darwin_indirect(make_link_flags_args):
    return _make_link_flags_darwin(make_link_flags_args, False)

def _make_link_flags_darwin_direct(make_link_flags_args):
    return _make_link_flags_darwin(make_link_flags_args, True)

def _make_link_flags_windows_msvc_indirect(make_link_flags_args):
    return _make_link_flags_windows_msvc(make_link_flags_args, False)

def _make_link_flags_windows_msvc_direct(make_link_flags_args):
    return _make_link_flags_windows_msvc(make_link_flags_args, True)

def _make_link_flags_windows_gnu_indirect(make_link_flags_args):
    return _make_link_flags_windows_gnu(make_link_flags_args, False)

def _make_link_flags_windows_gnu_direct(make_link_flags_args):
    return _make_link_flags_windows_gnu(make_link_flags_args, True)

def _get_make_link_flag_funcs(target_os, target_abi, use_direct_link_driver):
    """Select the appropriate functions for producing link arguments and library names.

    Args:
        target_os (str): The target platform triple system component.
        target_abi (str): The target platform triple abi component.
        use_direct_link_driver (bool): Whether or not linking is done directly via
            a link driver (`rust-lld`, `lld`, `lld-link.exe`, etc) vs indirectly via
            a compiler (`gcc`, `clang`, `clang-cl.exe`, etc)

    Returns:
        tuple:
            - callable: The function for producing link args.
            - callable: The function for formatting link library names.
    """
    if target_os == "windows":
        make_link_flags_windows_msvc = _make_link_flags_windows_msvc_direct if use_direct_link_driver else _make_link_flags_windows_msvc_indirect
        make_link_flags_windows_gnu = _make_link_flags_windows_gnu_direct if use_direct_link_driver else _make_link_flags_windows_gnu_indirect
        make_link_flags = make_link_flags_windows_msvc if target_abi == "msvc" else make_link_flags_windows_gnu
        get_lib_name = get_lib_name_for_windows
    elif target_os.startswith(("mac", "darwin", "ios")):
        make_link_flags_darwin = _make_link_flags_darwin_direct if use_direct_link_driver else _make_link_flags_darwin_indirect
        make_link_flags = make_link_flags_darwin
        get_lib_name = get_lib_name_default
    else:
        make_link_flags_default = _make_link_flags_default_direct if use_direct_link_driver else _make_link_flags_default_indirect
        make_link_flags = make_link_flags_default
        get_lib_name = get_lib_name_default

    return (make_link_flags, get_lib_name)

def _libraries_dirnames(make_link_flags_args):
    link_input, use_pic, _, _ = make_link_flags_args

    # De-duplicate names.
    return depset([get_preferred_artifact(lib, use_pic).dirname for lib in link_input.libraries]).to_list()

def _add_native_link_flags(
        args,
        dep_info,
        linkstamp_outs,
        ambiguous_libs,
        crate_type,
        toolchain,
        cc_toolchain,
        feature_configuration,
        compilation_mode,
        use_direct_link_driver,
        include_link_flags = True):
    """Adds linker flags for all dependencies of the current target.

    Args:
        args (Args): The Args struct for a ctx.action
        dep_info (DepInfo): Dependency Info provider
        linkstamp_outs (list): Linkstamp outputs of native dependencies
        ambiguous_libs (dict): Ambiguous libs, see `_disambiguate_libs`
        crate_type: Crate type of the current target
        toolchain (rust_toolchain): The current `rust_toolchain`
        cc_toolchain (CcToolchainInfo): The current `cc_toolchain`
        feature_configuration (FeatureConfiguration): feature configuration to use with cc_toolchain
        compilation_mode (bool): The compilation mode for this build.
        use_direct_link_driver (bool): Whether the linker is a direct driver (e.g. `ld`, `wasm-ld`) vs a wrapper (e.g. `clang`, `gcc`).
        include_link_flags (bool, optional): Whether to include flags like `-l` that instruct the linker to search for a library.
    """
    if crate_type in ["lib", "rlib"]:
        return

    use_pic = _should_use_pic(cc_toolchain, feature_configuration, crate_type, compilation_mode)

    make_link_flags, get_lib_name = _get_make_link_flag_funcs(
        target_os = toolchain.target_os,
        target_abi = toolchain.target_abi,
        use_direct_link_driver = use_direct_link_driver,
    )

    # TODO(hlopko): Remove depset flattening by using lambdas once we are on >=Bazel 5.0
    make_link_flags_args = [(arg, use_pic, ambiguous_libs, include_link_flags) for arg in dep_info.transitive_noncrates.to_list()]
    args.add_all(make_link_flags_args, map_each = _libraries_dirnames, uniquify = True, format_each = "-Lnative=%s")
    if ambiguous_libs:
        # If there are ambiguous libs, the disambiguation symlinks to them are
        # all created in the same directory. Add it to the library search path.
        ambiguous_libs_dirname = ambiguous_libs.values()[0].dirname
        args.add(ambiguous_libs_dirname, format = "-Lnative=%s")

    args.add_all(make_link_flags_args, map_each = make_link_flags)

    args.add_all(linkstamp_outs, format_each = "-Clink-args=%s")

    if cc_toolchain:
        if crate_type in ["dylib", "cdylib"]:
            # For shared libraries we want to link C++ runtime library dynamically
            # (for example libstdc++.so or libc++.so).
            args.add_all(
                cc_toolchain.dynamic_runtime_lib(feature_configuration = feature_configuration),
                map_each = _get_dirname,
                format_each = "-Lnative=%s",
            )
            if include_link_flags:
                args.add_all(
                    cc_toolchain.dynamic_runtime_lib(feature_configuration = feature_configuration),
                    map_each = get_lib_name,
                    format_each = "-ldylib=%s",
                )
        else:
            # For all other crate types we want to link C++ runtime library statically
            # (for example libstdc++.a or libc++.a).
            args.add_all(
                cc_toolchain.static_runtime_lib(feature_configuration = feature_configuration),
                map_each = _get_dirname,
                format_each = "-Lnative=%s",
            )
            if include_link_flags:
                args.add_all(
                    cc_toolchain.static_runtime_lib(feature_configuration = feature_configuration),
                    map_each = get_lib_name,
                    format_each = "-lstatic=%s",
                )

def _get_dirname(file):
    """A helper function for `_add_native_link_flags`.

    Args:
        file (File): The target file

    Returns:
        str: Directory name of `file`
    """
    return file.dirname

def _collect_per_crate_rustc_flags(ctx, crate_root, per_crate_rustc_flags):
    """Return all matching per-crate rustc flags.

    Args:
        ctx (ctx): The source rule's context object
        crate_root (File): The root file of the crate
        per_crate_rustc_flags (list): A list of per_crate_rustc_flag values

    Returns:
        List[str]: matching per-crate rustc flags
    """

    flags = []

    for per_crate_rustc_flag in per_crate_rustc_flags:
        at_index = per_crate_rustc_flag.find("@")
        if at_index == -1:
            fail("per_crate_rustc_flag '{}' does not follow the expected format: prefix_filter@flag".format(per_crate_rustc_flag))

        prefix_filter = per_crate_rustc_flag[:at_index]
        flag = per_crate_rustc_flag[at_index + 1:]
        if not flag:
            fail("per_crate_rustc_flag '{}' does not follow the expected format: prefix_filter@flag".format(per_crate_rustc_flag))

        label_string = str(ctx.label)
        if label_string.startswith("@//"):
            label = label_string[1:]
        elif label_string.startswith("@@//"):
            label = label_string[2:]
        else:
            label = label_string
        execution_path = crate_root.path

        if label.startswith(prefix_filter) or execution_path.startswith(prefix_filter):
            flags.append(flag)

    return flags

def _error_format_impl(ctx):
    """Implementation of the `error_format` rule

    Args:
        ctx (ctx): The rule's context object

    Returns:
        list: A list containing the ErrorFormatInfo provider
    """
    raw = ctx.build_setting_value
    if raw not in _error_format_values:
        fail("{} expected a value in `{}` but got `{}`".format(
            ctx.label,
            _error_format_values,
            raw,
        ))
    return [ErrorFormatInfo(error_format = raw)]

error_format = rule(
    doc = (
        "Change the [--error-format](https://doc.rust-lang.org/rustc/command-line-arguments.html#option-error-format) " +
        "flag from the command line with `--@rules_rust//rust/settings:error_format`. See rustc documentation for valid values."
    ),
    implementation = _error_format_impl,
    build_setting = config.string(flag = True),
)

def get_error_format(attr, attr_name):
    if hasattr(attr, attr_name):
        return getattr(attr, attr_name)[ErrorFormatInfo].error_format
    return "human"

def _always_enable_metadata_output_groups_impl(ctx):
    """Implementation of the `always_enable_metadata_output_groups` rule

    Args:
        ctx (ctx): The rule's context object

    Returns:
        list: A list containing the AlwaysEnableMetadataOutputGroupsInfo provider
    """
    return [AlwaysEnableMetadataOutputGroupsInfo(
        always_enable_metadata_output_groups = ctx.build_setting_value,
    )]

always_enable_metadata_output_groups = rule(
    doc = (
        "Setting this flag from the command line with `--@rules_rust//rust/settings:always_enable_metadata_output_groups` " +
        "will cause all rules to generate the `metadata` and `rustc_rmeta_output` output groups, " +
        "rather than the default behavior of only generated them for libraries when pipelined " +
        "compilation is enabled."
    ),
    implementation = _always_enable_metadata_output_groups_impl,
    build_setting = config.bool(flag = True),
)

def _rustc_output_diagnostics_impl(ctx):
    """Implementation of the `rustc_output_diagnostics` rule

    Args:
        ctx (ctx): The rule's context object

    Returns:
        list: A list containing the RustcOutputDiagnosticsInfo provider
    """
    return [RustcOutputDiagnosticsInfo(
        rustc_output_diagnostics = ctx.build_setting_value,
    )]

rustc_output_diagnostics = rule(
    doc = (
        "Setting this flag from the command line with `--@rules_rust//rust/settings:rustc_output_diagnostics` " +
        "makes rules_rust save rustc json output(suitable for consumption by rust-analyzer) in a file. " +
        "These are accessible via the `rustc_rmeta_output` (for pipelined compilation or if " +
        "`always_enable_metadata_output_groups` is enabled) and `rustc_output` output groups. " +
        "You can find these using `bazel cquery`"
    ),
    implementation = _rustc_output_diagnostics_impl,
    build_setting = config.bool(flag = True),
)

def _extra_rustc_env_impl(ctx):
    env_vars = parse_env_strings(ctx.build_setting_value)
    return ExtraRustcEnvInfo(extra_rustc_env = env_vars)

extra_rustc_env = rule(
    doc = (
        "Add additional environment variables to rustc in non-exec configuration using " +
        "`--@rules_rust//rust/settings:extra_rustc_env=FOO=bar`. Multiple values may be specified."
    ),
    implementation = _extra_rustc_env_impl,
    build_setting = config.string_list(flag = True),
)

def _extra_rustc_flags_impl(ctx):
    return ExtraRustcFlagsInfo(extra_rustc_flags = ctx.build_setting_value)

extra_rustc_flags = rule(
    doc = (
        "Add additional rustc_flags from the command line with `--@rules_rust//rust/settings:extra_rustc_flags`. " +
        "This flag should only be used for flags that need to be applied across the entire build. For options that " +
        "apply to individual crates, use the rustc_flags attribute on the individual crate's rule instead. NOTE: " +
        "These flags not applied to the exec configuration (proc-macros, cargo_build_script, etc); " +
        "use `--@rules_rust//rust/settings:extra_exec_rustc_flags` to apply flags to the exec configuration."
    ),
    implementation = _extra_rustc_flags_impl,
    build_setting = config.string_list(flag = True),
)

def _extra_rustc_flag_impl(ctx):
    return ExtraRustcFlagsInfo(extra_rustc_flags = [f for f in ctx.build_setting_value if f != ""])

extra_rustc_flag = rule(
    doc = (
        "Add additional rustc_flag from the command line with `--@rules_rust//rust/settings:extra_rustc_flag`. " +
        "Multiple uses are accumulated and appended after the extra_rustc_flags."
    ),
    implementation = _extra_rustc_flag_impl,
    build_setting = config.string_list(flag = True, repeatable = True),
)

def _extra_exec_rustc_flags_impl(ctx):
    return ExtraExecRustcFlagsInfo(extra_exec_rustc_flags = ctx.build_setting_value)

extra_exec_rustc_flags = rule(
    doc = (
        "Add additional rustc_flags in the exec configuration from the command line with `--@rules_rust//rust/settings:extra_exec_rustc_flags`. " +
        "This flag should only be used for flags that need to be applied across the entire build. " +
        "These flags only apply to the exec configuration (proc-macros, cargo_build_script, etc)."
    ),
    implementation = _extra_exec_rustc_flags_impl,
    build_setting = config.string_list(flag = True),
)

def _extra_exec_rustc_env_impl(ctx):
    env_vars = parse_env_strings(ctx.build_setting_value)
    return ExtraExecRustcEnvInfo(extra_exec_rustc_env = env_vars)

extra_exec_rustc_env = rule(
    doc = (
        "Add additional environment variables to rustc in non-exec configuration using " +
        "`--@rules_rust//rust/settings:extra_exec_rustc_env=FOO=bar`. Multiple values may be specified."
    ),
    implementation = _extra_exec_rustc_env_impl,
    build_setting = config.string_list(flag = True),
)

def _extra_exec_rustc_flag_impl(ctx):
    return ExtraExecRustcFlagsInfo(extra_exec_rustc_flags = [f for f in ctx.build_setting_value if f != ""])

extra_exec_rustc_flag = rule(
    doc = (
        "Add additional rustc_flags in the exec configuration from the command line with `--@rules_rust//rust/settings:extra_exec_rustc_flag`. " +
        "Multiple uses are accumulated and appended after the extra_exec_rustc_flags."
    ),
    implementation = _extra_exec_rustc_flag_impl,
    build_setting = config.string_list(flag = True, repeatable = True),
)

def _per_crate_rustc_flag_impl(ctx):
    return PerCrateRustcFlagsInfo(per_crate_rustc_flags = [f for f in ctx.build_setting_value if f != ""])

per_crate_rustc_flag = rule(
    doc = (
        "Add additional rustc_flag to matching crates from the command line with `--@rules_rust//rust/settings:experimental_per_crate_rustc_flag`. " +
        "The expected flag format is prefix_filter@flag, where any crate with a label or execution path starting with the prefix filter will be built with the given flag." +
        "The label matching uses the canonical form of the label (i.e //package:label_name)." +
        "The execution path is the relative path to your workspace directory including the base name (including extension) of the crate root." +
        "This flag is only applied to the exec configuration (proc-macros, cargo_build_script, etc)." +
        "Multiple uses are accumulated."
    ),
    implementation = _per_crate_rustc_flag_impl,
    build_setting = config.string_list(flag = True, repeatable = True),
)

def _no_std_impl(ctx):
    value = str(ctx.attr._no_std[BuildSettingInfo].value)
    if is_exec_configuration(ctx):
        return [config_common.FeatureFlagInfo(value = "off")]
    return [config_common.FeatureFlagInfo(value = value)]

no_std = rule(
    doc = (
        "No std; we need this so that we can distinguish between host and exec"
    ),
    attrs = {
        "_no_std": attr.label(default = "//rust/settings:no_std"),
    },
    implementation = _no_std_impl,
)
