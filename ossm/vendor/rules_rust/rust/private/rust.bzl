# Copyright 2015 The Bazel Authors. All rights reserved.
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

"""Rust rule implementations"""

load("@bazel_skylib//lib:paths.bzl", "paths")
load("//rust/private:common.bzl", "COMMON_PROVIDERS", "rust_common")
load("//rust/private:providers.bzl", "BuildInfo")
load("//rust/private:rustc.bzl", "rustc_compile_action")
load(
    "//rust/private:utils.bzl",
    "can_build_metadata",
    "compute_crate_name",
    "crate_root_src",
    "dedent",
    "determine_lib_name",
    "determine_output_hash",
    "expand_dict_value_locations",
    "find_toolchain",
    "generate_output_diagnostics",
    "get_edition",
    "get_import_macro_deps",
    "transform_deps",
    "transform_sources",
)

# TODO(marco): Separate each rule into its own file.

def _assert_no_deprecated_attributes(_ctx):
    """Forces a failure if any deprecated attributes were specified

    Args:
        _ctx (ctx): The current rule's context object
    """
    pass

def _assert_correct_dep_mapping(ctx):
    """Forces a failure if proc_macro_deps and deps are mixed inappropriately

    Args:
        ctx (ctx): The current rule's context object
    """
    for dep in ctx.attr.deps:
        if rust_common.crate_info in dep:
            if dep[rust_common.crate_info].type == "proc-macro":
                fail(
                    "{} listed {} in its deps, but it is a proc-macro. It should instead be in the bazel property proc_macro_deps.".format(
                        ctx.label,
                        dep.label,
                    ),
                )
    for dep in ctx.attr.proc_macro_deps:
        type = dep[rust_common.crate_info].type
        if type != "proc-macro":
            fail(
                "{} listed {} in its proc_macro_deps, but it is not proc-macro, it is a {}. It should probably instead be listed in deps.".format(
                    ctx.label,
                    dep.label,
                    type,
                ),
            )

def _rust_library_impl(ctx):
    """The implementation of the `rust_library` rule.

    This rule provides CcInfo, so it can be used everywhere Bazel
    expects rules_cc, but care must be taken to have the correct
    dependencies on an allocator and std implemetation as needed.

    Args:
        ctx (ctx): The rule's context object

    Returns:
        list: A list of providers.
    """
    return _rust_library_common(ctx, "rlib")

def _rust_static_library_impl(ctx):
    """The implementation of the `rust_static_library` rule.

    This rule provides CcInfo, so it can be used everywhere Bazel
    expects rules_cc.

    Args:
        ctx (ctx): The rule's context object

    Returns:
        list: A list of providers.
    """
    return _rust_library_common(ctx, "staticlib")

def _rust_shared_library_impl(ctx):
    """The implementation of the `rust_shared_library` rule.

    This rule provides CcInfo, so it can be used everywhere Bazel
    expects rules_cc.

    On Windows, a PDB file containing debugging information is available under
    the key `pdb_file` in `OutputGroupInfo`. Similarly on macOS, a dSYM folder
    is available under the key `dsym_folder` in `OutputGroupInfo`.

    Args:
        ctx (ctx): The rule's context object

    Returns:
        list: A list of providers.
    """
    return _rust_library_common(ctx, "cdylib")

def _rust_proc_macro_impl(ctx):
    """The implementation of the `rust_proc_macro` rule.

    Args:
        ctx (ctx): The rule's context object

    Returns:
        list: A list of providers.
    """
    return _rust_library_common(ctx, "proc-macro")

def _rust_library_common(ctx, crate_type):
    """The common implementation of the library-like rules.

    Args:
        ctx (ctx): The rule's context object
        crate_type (String): one of lib|rlib|dylib|staticlib|cdylib|proc-macro

    Returns:
        list: A list of providers. See `rustc_compile_action`
    """
    _assert_no_deprecated_attributes(ctx)
    _assert_correct_dep_mapping(ctx)

    toolchain = find_toolchain(ctx)

    crate_name = compute_crate_name(ctx.workspace_name, ctx.label, toolchain, ctx.attr.crate_name)

    crate_root = getattr(ctx.file, "crate_root", None)
    if not crate_root:
        crate_root = crate_root_src(ctx.attr.name, ctx.attr.crate_name, ctx.files.srcs, crate_type)
    srcs, crate_root = transform_sources(ctx, ctx.files.srcs, crate_root)

    # Determine unique hash for this rlib.
    # Note that we don't include a hash for `cdylib` and `staticlib` since they are meant to be consumed externally
    # and having a deterministic name is important since it ends up embedded in the executable. This is problematic
    # when one needs to include the library with a specific filename into a larger application.
    # (see https://github.com/bazelbuild/rules_rust/issues/405#issuecomment-993089889 for more details)
    if crate_type in ["cdylib", "staticlib"]:
        output_hash = None
    else:
        output_hash = determine_output_hash(crate_root, ctx.label)

    rust_lib_name = determine_lib_name(
        crate_name,
        crate_type,
        toolchain,
        output_hash,
    )
    rust_lib = ctx.actions.declare_file(rust_lib_name)
    rust_metadata = None
    rustc_rmeta_output = None
    if can_build_metadata(toolchain, ctx, crate_type) and not ctx.attr.disable_pipelining:
        rust_metadata = ctx.actions.declare_file(
            paths.replace_extension(rust_lib_name, ".rmeta"),
            sibling = rust_lib,
        )
        rustc_rmeta_output = generate_output_diagnostics(ctx, rust_metadata)

    deps = transform_deps(ctx.attr.deps)
    proc_macro_deps = transform_deps(ctx.attr.proc_macro_deps + get_import_macro_deps(ctx))

    return rustc_compile_action(
        ctx = ctx,
        attr = ctx.attr,
        toolchain = toolchain,
        output_hash = output_hash,
        crate_info_dict = dict(
            name = crate_name,
            type = crate_type,
            root = crate_root,
            srcs = depset(srcs),
            deps = depset(deps),
            proc_macro_deps = depset(proc_macro_deps),
            aliases = ctx.attr.aliases,
            output = rust_lib,
            rustc_output = generate_output_diagnostics(ctx, rust_lib),
            metadata = rust_metadata,
            rustc_rmeta_output = rustc_rmeta_output,
            edition = get_edition(ctx.attr, toolchain, ctx.label),
            rustc_env = ctx.attr.rustc_env,
            rustc_env_files = ctx.files.rustc_env_files,
            is_test = False,
            data = depset(ctx.files.data),
            compile_data = depset(ctx.files.compile_data),
            compile_data_targets = depset(ctx.attr.compile_data),
            owner = ctx.label,
        ),
    )

def _rust_binary_impl(ctx):
    """The implementation of the `rust_binary` rule

    Args:
        ctx (ctx): The rule's context object

    Returns:
        list: A list of providers. See `rustc_compile_action`
    """
    toolchain = find_toolchain(ctx)
    crate_name = compute_crate_name(ctx.workspace_name, ctx.label, toolchain, ctx.attr.crate_name)
    _assert_correct_dep_mapping(ctx)

    if ctx.attr.binary_name:
        output_filename = ctx.attr.binary_name
    else:
        output_filename = ctx.label.name
    output = ctx.actions.declare_file(output_filename + toolchain.binary_ext)

    deps = transform_deps(ctx.attr.deps)
    proc_macro_deps = transform_deps(ctx.attr.proc_macro_deps + get_import_macro_deps(ctx))

    crate_root = getattr(ctx.file, "crate_root", None)
    if not crate_root:
        crate_root = crate_root_src(ctx.attr.name, ctx.attr.crate_name, ctx.files.srcs, ctx.attr.crate_type)
    srcs, crate_root = transform_sources(ctx, ctx.files.srcs, crate_root)

    providers = rustc_compile_action(
        ctx = ctx,
        attr = ctx.attr,
        toolchain = toolchain,
        crate_info_dict = dict(
            name = crate_name,
            type = ctx.attr.crate_type,
            root = crate_root,
            srcs = depset(srcs),
            deps = depset(deps),
            proc_macro_deps = depset(proc_macro_deps),
            aliases = ctx.attr.aliases,
            output = output,
            rustc_output = generate_output_diagnostics(ctx, output),
            edition = get_edition(ctx.attr, toolchain, ctx.label),
            rustc_env = ctx.attr.rustc_env,
            rustc_env_files = ctx.files.rustc_env_files,
            is_test = False,
            data = depset(ctx.files.data),
            compile_data = depset(ctx.files.compile_data),
            compile_data_targets = depset(ctx.attr.compile_data),
            owner = ctx.label,
        ),
    )

    providers.append(RunEnvironmentInfo(
        environment = expand_dict_value_locations(
            ctx,
            ctx.attr.env,
            ctx.attr.data,
            {},
        ),
    ))

    return providers

def get_rust_test_flags(attr):
    """Determine the desired rustc flags for test targets.

    Args:
        attr (dict): Attributes of a rule

    Returns:
        List: A list of test flags
    """
    if getattr(attr, "use_libtest_harness", True):
        rust_flags = ["--test"]
    else:
        rust_flags = ["--cfg", "test"]

    return rust_flags

def _rust_test_impl(ctx):
    """The implementation of the `rust_test` rule.

    Args:
        ctx (ctx): The ctx object for the current target.

    Returns:
        list: The list of providers. See `rustc_compile_action`
    """
    _assert_no_deprecated_attributes(ctx)
    _assert_correct_dep_mapping(ctx)

    toolchain = find_toolchain(ctx)

    crate_type = "bin"
    deps = transform_deps(ctx.attr.deps)
    proc_macro_deps = transform_deps(ctx.attr.proc_macro_deps + get_import_macro_deps(ctx))

    if ctx.attr.crate and ctx.attr.srcs:
        fail("rust_test.crate and rust_test.srcs are mutually exclusive. Update {} to use only one of these attributes".format(
            ctx.label,
        ))

    if ctx.attr.crate:
        # Target is building the crate in `test` config
        crate = ctx.attr.crate[rust_common.crate_info] if rust_common.crate_info in ctx.attr.crate else ctx.attr.crate[rust_common.test_crate_info].crate

        if toolchain._incompatible_change_rust_test_compilation_output_directory:
            crate_name = compute_crate_name(ctx.workspace_name, ctx.label, toolchain, ctx.attr.crate_name)
            output = ctx.actions.declare_file(
                ctx.label.name + toolchain.binary_ext,
            )
        else:
            crate_name = crate.name
            output_hash = determine_output_hash(crate.root, ctx.label)
            output = ctx.actions.declare_file(
                "test-%s/%s%s" % (
                    output_hash,
                    ctx.label.name,
                    toolchain.binary_ext,
                ),
            )

        srcs, crate_root = transform_sources(ctx, ctx.files.srcs, getattr(ctx.file, "crate_root", None))

        # Optionally join compile data
        if crate.compile_data:
            compile_data = depset(ctx.files.compile_data, transitive = [crate.compile_data])
        else:
            compile_data = depset(ctx.files.compile_data)
        if crate.compile_data_targets:
            compile_data_targets = depset(ctx.attr.compile_data, transitive = [crate.compile_data_targets])
        else:
            compile_data_targets = depset(ctx.attr.compile_data)
        rustc_env_files = ctx.files.rustc_env_files + crate.rustc_env_files

        # crate.rustc_env is already expanded upstream in rust_library rule implementation
        rustc_env = dict(crate.rustc_env)
        data_paths = depset(direct = getattr(ctx.attr, "data", [])).to_list()
        rustc_env.update(expand_dict_value_locations(
            ctx,
            ctx.attr.rustc_env,
            data_paths,
            {},
        ))
        aliases = dict(crate.aliases)
        aliases.update(ctx.attr.aliases)

        # Build the test binary using the dependency's srcs.
        crate_info_dict = dict(
            name = crate_name,
            type = crate_type,
            root = crate.root,
            srcs = depset(srcs, transitive = [crate.srcs]),
            deps = depset(deps, transitive = [crate.deps]),
            proc_macro_deps = depset(proc_macro_deps, transitive = [crate.proc_macro_deps]),
            aliases = aliases,
            output = output,
            rustc_output = generate_output_diagnostics(ctx, output),
            edition = crate.edition,
            rustc_env = rustc_env,
            rustc_env_files = rustc_env_files,
            is_test = True,
            compile_data = compile_data,
            compile_data_targets = compile_data_targets,
            wrapped_crate_type = crate.type,
            owner = ctx.label,
        )
    else:
        crate_root = getattr(ctx.file, "crate_root", None)

        if not crate_root:
            crate_root_type = "lib" if ctx.attr.use_libtest_harness else "bin"
            crate_root = crate_root_src(ctx.attr.name, ctx.attr.crate_name, ctx.files.srcs, crate_root_type)
        srcs, crate_root = transform_sources(ctx, ctx.files.srcs, crate_root)

        if toolchain._incompatible_change_rust_test_compilation_output_directory:
            output = ctx.actions.declare_file(
                ctx.label.name + toolchain.binary_ext,
            )
        else:
            output_hash = determine_output_hash(crate_root, ctx.label)
            output = ctx.actions.declare_file(
                "test-%s/%s%s" % (
                    output_hash,
                    ctx.label.name,
                    toolchain.binary_ext,
                ),
            )

        data_paths = depset(direct = getattr(ctx.attr, "data", [])).to_list()
        rustc_env = expand_dict_value_locations(
            ctx,
            ctx.attr.rustc_env,
            data_paths,
            {},
        )

        # Target is a standalone crate. Build the test binary as its own crate.
        crate_info_dict = dict(
            name = compute_crate_name(ctx.workspace_name, ctx.label, toolchain, ctx.attr.crate_name),
            type = crate_type,
            root = crate_root,
            srcs = depset(srcs),
            deps = depset(deps),
            proc_macro_deps = depset(proc_macro_deps),
            aliases = ctx.attr.aliases,
            output = output,
            rustc_output = generate_output_diagnostics(ctx, output),
            edition = get_edition(ctx.attr, toolchain, ctx.label),
            rustc_env = rustc_env,
            rustc_env_files = ctx.files.rustc_env_files,
            is_test = True,
            compile_data = depset(ctx.files.compile_data),
            compile_data_targets = depset(ctx.attr.compile_data),
            owner = ctx.label,
        )

    providers = rustc_compile_action(
        ctx = ctx,
        attr = ctx.attr,
        toolchain = toolchain,
        crate_info_dict = crate_info_dict,
        rust_flags = get_rust_test_flags(ctx.attr),
        skip_expanding_rustc_env = True,
    )
    data = getattr(ctx.attr, "data", [])

    env = expand_dict_value_locations(
        ctx,
        getattr(ctx.attr, "env", {}),
        data,
        {},
    )
    if toolchain.llvm_cov and ctx.configuration.coverage_enabled:
        if not toolchain.llvm_profdata:
            fail("toolchain.llvm_profdata is required if toolchain.llvm_cov is set.")

        if toolchain._experimental_use_coverage_metadata_files:
            llvm_cov_path = toolchain.llvm_cov.path
            llvm_profdata_path = toolchain.llvm_profdata.path
        else:
            llvm_cov_path = toolchain.llvm_cov.short_path
            if llvm_cov_path.startswith("../"):
                llvm_cov_path = llvm_cov_path[len("../"):]

            llvm_profdata_path = toolchain.llvm_profdata.short_path
            if llvm_profdata_path.startswith("../"):
                llvm_profdata_path = llvm_profdata_path[len("../"):]

        env["RUST_LLVM_COV"] = llvm_cov_path
        env["RUST_LLVM_PROFDATA"] = llvm_profdata_path
    components = "{}/{}".format(ctx.label.workspace_root, ctx.label.package).split("/")
    env["CARGO_MANIFEST_DIR"] = "/".join([c for c in components if c])
    providers.append(RunEnvironmentInfo(
        environment = env,
        inherited_environment = ctx.attr.env_inherit,
    ))

    return providers

def _rust_library_group_impl(ctx):
    dep_variant_infos = []
    dep_variant_transitive_infos = []
    runfiles = []

    for dep in ctx.attr.deps:
        if rust_common.crate_info in dep:
            dep_variant_infos.append(rust_common.dep_variant_info(
                crate_info = dep[rust_common.crate_info] if rust_common.crate_info in dep else None,
                dep_info = dep[rust_common.dep_info] if rust_common.crate_info in dep else None,
                build_info = dep[BuildInfo] if BuildInfo in dep else None,
                cc_info = dep[CcInfo] if CcInfo in dep else None,
                crate_group_info = None,
            ))
        elif rust_common.crate_group_info in dep:
            dep_variant_transitive_infos.append(dep[rust_common.crate_group_info].dep_variant_infos)
        else:
            fail("crate_group_info targets can only depend on rust_library or rust_library_group targets.")

        if dep[DefaultInfo].default_runfiles != None:
            runfiles.append(dep[DefaultInfo].default_runfiles)

    return [
        rust_common.crate_group_info(
            dep_variant_infos = depset(dep_variant_infos, transitive = dep_variant_transitive_infos),
        ),
        DefaultInfo(runfiles = ctx.runfiles().merge_all(runfiles)),
        coverage_common.instrumented_files_info(
            ctx,
            dependency_attributes = ["deps"],
        ),
    ]

def _stamp_attribute(default_value):
    return attr.int(
        doc = dedent("""\
            Whether to encode build information into the `Rustc` action. Possible values:

            - `stamp = 1`: Always stamp the build information into the `Rustc` action, even in \
            [--nostamp](https://docs.bazel.build/versions/main/user-manual.html#flag--stamp) builds. \
            This setting should be avoided, since it potentially kills remote caching for the target and \
            any downstream actions that depend on it.

            - `stamp = 0`: Always replace build information by constant values. This gives good build result caching.

            - `stamp = -1`: Embedding of build information is controlled by the \
            [--[no]stamp](https://docs.bazel.build/versions/main/user-manual.html#flag--stamp) flag.

            Stamped targets are not rebuilt unless their dependencies change.

            For example if a `rust_library` is stamped, and a `rust_binary` depends on that library, the stamped
            library won't be rebuilt when we change sources of the `rust_binary`. This is different from how
            [`cc_library.linkstamps`](https://docs.bazel.build/versions/main/be/c-cpp.html#cc_library.linkstamp)
            behaves.
        """),
        default = default_value,
        values = [1, 0, -1],
    )

# Internal attributes core to Rustc actions.
RUSTC_ATTRS = {
    "_cc_toolchain": attr.label(
        doc = (
            "In order to use find_cc_toolchain, your rule has to depend " +
            "on C++ toolchain. See `@rules_cc//cc:find_cc_toolchain.bzl` " +
            "docs for details."
        ),
        default = Label("@bazel_tools//tools/cpp:current_cc_toolchain"),
    ),
    "_error_format": attr.label(
        default = Label("//rust/settings:error_format"),
    ),
    "_extra_exec_rustc_flag": attr.label(
        default = Label("//rust/settings:extra_exec_rustc_flag"),
    ),
    "_extra_exec_rustc_flags": attr.label(
        default = Label("//rust/settings:extra_exec_rustc_flags"),
    ),
    "_extra_rustc_flag": attr.label(
        default = Label("//rust/settings:extra_rustc_flag"),
    ),
    "_extra_rustc_flags": attr.label(
        default = Label("//rust/settings:extra_rustc_flags"),
    ),
    "_is_proc_macro_dep": attr.label(
        default = Label("//rust/private:is_proc_macro_dep"),
    ),
    "_is_proc_macro_dep_enabled": attr.label(
        default = Label("//rust/private:is_proc_macro_dep_enabled"),
    ),
    "_per_crate_rustc_flag": attr.label(
        default = Label("//rust/settings:experimental_per_crate_rustc_flag"),
    ),
    "_process_wrapper": attr.label(
        doc = "A process wrapper for running rustc on all platforms.",
        default = Label("//util/process_wrapper"),
        executable = True,
        allow_single_file = True,
        cfg = "exec",
    ),
    "_rustc_output_diagnostics": attr.label(
        default = Label("//rust/settings:rustc_output_diagnostics"),
    ),
}

_common_attrs = {
    "aliases": attr.label_keyed_string_dict(
        doc = dedent("""\
            Remap crates to a new name or moniker for linkage to this target

            These are other `rust_library` targets and will be presented as the new name given.
        """),
    ),
    "alwayslink": attr.bool(
        doc = dedent("""\
            If 1, any binary that depends (directly or indirectly) on this library
            will link in all the object files even if some contain no symbols referenced by the binary.

            This attribute is used by the C++ Starlark API when passing CcInfo providers.
        """),
        default = False,
    ),
    "compile_data": attr.label_list(
        doc = dedent("""\
            List of files used by this rule at compile time.

            This attribute can be used to specify any data files that are embedded into
            the library, such as via the
            [`include_str!`](https://doc.rust-lang.org/std/macro.include_str!.html)
            macro.
        """),
        allow_files = True,
    ),
    "crate_features": attr.string_list(
        doc = dedent("""\
            List of features to enable for this crate.

            Features are defined in the code using the `#[cfg(feature = "foo")]`
            configuration option. The features listed here will be passed to `rustc`
            with `--cfg feature="${feature_name}"` flags.
        """),
    ),
    "crate_name": attr.string(
        doc = dedent("""\
            Crate name to use for this target.

            This must be a valid Rust identifier, i.e. it may contain only alphanumeric characters and underscores.
            Defaults to the target name, with any hyphens replaced by underscores.
        """),
    ),
    "crate_root": attr.label(
        doc = dedent("""\
            The file that will be passed to `rustc` to be used for building this crate.

            If `crate_root` is not set, then this rule will look for a `lib.rs` file (or `main.rs` for rust_binary)
            or the single file in `srcs` if `srcs` contains only one file.
        """),
        allow_single_file = [".rs"],
    ),
    "data": attr.label_list(
        doc = dedent("""\
            List of files used by this rule at compile time and runtime.

            If including data at compile time with include_str!() and similar,
            prefer `compile_data` over `data`, to prevent the data also being included
            in the runfiles.
        """),
        allow_files = True,
    ),
    "deps": attr.label_list(
        doc = dedent("""\
            List of other libraries to be linked to this library target.

            These can be either other `rust_library` targets or `cc_library` targets if
            linking a native library.
        """),
    ),
    "edition": attr.string(
        doc = "The rust edition to use for this crate. Defaults to the edition specified in the rust_toolchain.",
    ),
    # Previously `proc_macro_deps` were a part of `deps`, and then proc_macro_host_transition was
    # used into cfg="host" using `@local_config_platform//:host`.
    # This fails for remote execution, which needs cfg="exec", and there isn't anything like
    # `@local_config_platform//:exec` exposed.
    "proc_macro_deps": attr.label_list(
        doc = dedent("""\
            List of `rust_proc_macro` targets used to help build this library target.
        """),
        cfg = "exec",
        providers = [rust_common.crate_info],
    ),
    "rustc_env": attr.string_dict(
        doc = dedent("""\
            Dictionary of additional `"key": "value"` environment variables to set for rustc.

            rust_test()/rust_binary() rules can use $(rootpath //package:target) to pass in the
            location of a generated file or external tool. Cargo build scripts that wish to
            expand locations should use cargo_build_script()'s build_script_env argument instead,
            as build scripts are run in a different environment - see cargo_build_script()'s
            documentation for more.
        """),
    ),
    "rustc_env_files": attr.label_list(
        doc = dedent("""\
            Files containing additional environment variables to set for rustc.

            These files should  contain a single variable per line, of format
            `NAME=value`, and newlines may be included in a value by ending a
            line with a trailing back-slash (`\\\\`).

            The order that these files will be processed is unspecified, so
            multiple definitions of a particular variable are discouraged.

            Note that the variables here are subject to
            [workspace status](https://docs.bazel.build/versions/main/user-manual.html#workspace_status)
            stamping should the `stamp` attribute be enabled. Stamp variables
            should be wrapped in brackets in order to be resolved. E.g.
            `NAME={WORKSPACE_STATUS_VARIABLE}`.
        """),
        allow_files = True,
    ),
    "rustc_flags": attr.string_list(
        doc = dedent("""\
            List of compiler flags passed to `rustc`.

            These strings are subject to Make variable expansion for predefined
            source/output path variables like `$location`, `$execpath`, and
            `$rootpath`. This expansion is useful if you wish to pass a generated
            file of arguments to rustc: `@$(location //package:target)`.
        """),
    ),
    # TODO(stardoc): How do we provide additional documentation to an inherited attribute?
    # "name": attr.string(
    #     doc = "This name will also be used as the name of the crate built by this rule.",
    # `),
    "srcs": attr.label_list(
        doc = dedent("""\
            List of Rust `.rs` source files used to build the library.

            If `srcs` contains more than one file, then there must be a file either
            named `lib.rs`. Otherwise, `crate_root` must be set to the source file that
            is the root of the crate to be passed to rustc to build this crate.
        """),
        allow_files = [".rs"],
        # Allow use of --compile_one_dependency with rust targets. Support for this feature for
        # non-builtin rulesets is undocumented outside of the bazel source:
        # https://github.com/bazelbuild/bazel/blob/7.1.1/src/main/java/com/google/devtools/build/lib/packages/Attribute.java#L102
        flags = ["DIRECT_COMPILE_TIME_INPUT"],
    ),
    "stamp": _stamp_attribute(
        default_value = 0,
    ),
    "version": attr.string(
        doc = "A version to inject in the cargo environment variable.",
        default = "0.0.0",
    ),
    "_stamp_flag": attr.label(
        doc = "A setting used to determine whether or not the `--stamp` flag is enabled",
        default = Label("//rust/private:stamp"),
    ),
} | RUSTC_ATTRS

_coverage_attrs = {
    "_collect_cc_coverage": attr.label(
        default = Label("//util/collect_coverage"),
        executable = True,
        cfg = "exec",
    ),
    # Bazel’s coverage runner
    # (https://github.com/bazelbuild/bazel/blob/6.0.0/tools/test/collect_coverage.sh)
    # needs a binary called “lcov_merge.”  Its location is passed in the
    # LCOV_MERGER environmental variable.  For builtin rules, this variable
    # is set automatically based on a magic “$lcov_merger” or
    # “:lcov_merger” attribute, but it’s not possible to create such
    # attributes in Starlark.  Therefore we specify the variable ourselves.
    # Note that the coverage runner runs in the runfiles root instead of
    # the execution root, therefore we use “path” instead of “short_path.”
    "_lcov_merger": attr.label(
        default = configuration_field(fragment = "coverage", name = "output_generator"),
        executable = True,
        cfg = "exec",
    ),
}

_experimental_use_cc_common_link_attrs = {
    "experimental_use_cc_common_link": attr.int(
        doc = (
            "Whether to use cc_common.link to link rust binaries. " +
            "Possible values: [-1, 0, 1]. " +
            "-1 means use the value of the toolchain.experimental_use_cc_common_link " +
            "boolean build setting to determine. " +
            "0 means do not use cc_common.link (use rustc instead). " +
            "1 means use cc_common.link."
        ),
        values = [-1, 0, 1],
        default = -1,
    ),
    "malloc": attr.label(
        default = Label("@bazel_tools//tools/cpp:malloc"),
        doc = """Override the default dependency on `malloc`.

By default, Rust binaries linked with cc_common.link are linked against
`@bazel_tools//tools/cpp:malloc"`, which is an empty library and the resulting binary will use
libc's `malloc`. This label must refer to a `cc_library` rule.
""",
        mandatory = False,
        providers = [[CcInfo]],
    ),  # A late-bound attribute denoting the value of the `--custom_malloc`
    # command line flag (or None if the flag is not provided).
    "_custom_malloc": attr.label(
        default = configuration_field(
            fragment = "cpp",
            name = "custom_malloc",
        ),
        providers = [[CcInfo]],
    ),
}

_rust_test_attrs = {
    "crate": attr.label(
        mandatory = False,
        doc = dedent("""\
            Target inline tests declared in the given crate

            These tests are typically those that would be held out under
            `#[cfg(test)]` declarations.
        """),
    ),
    "env": attr.string_dict(
        mandatory = False,
        doc = dedent("""\
            Specifies additional environment variables to set when the test is executed by bazel test.
            Values are subject to `$(rootpath)`, `$(execpath)`, location, and
            ["Make variable"](https://docs.bazel.build/versions/master/be/make-variables.html) substitution.
        """),
    ),
    "env_inherit": attr.string_list(
        doc = "Specifies additional environment variables to inherit from the external environment when the test is executed by bazel test.",
    ),
    "use_libtest_harness": attr.bool(
        mandatory = False,
        default = True,
        doc = dedent("""\
            Whether to use `libtest`. For targets using this flag, individual tests can be run by using the
            [--test_arg](https://docs.bazel.build/versions/4.0.0/command-line-reference.html#flag--test_arg) flag.
            E.g. `bazel test //src:rust_test --test_arg=foo::test::test_fn`.
        """),
    ),
    "_use_grep_includes": attr.bool(default = True),
} | _coverage_attrs | _experimental_use_cc_common_link_attrs

rust_library = rule(
    implementation = _rust_library_impl,
    provides = COMMON_PROVIDERS,
    attrs = _common_attrs | {
        "disable_pipelining": attr.bool(
            default = False,
            doc = dedent("""\
                Disables pipelining for this rule if it is globally enabled.
                This will cause this rule to not produce a `.rmeta` file and all the dependent
                crates will instead use the `.rlib` file.
            """),
        ),
    },
    fragments = ["cpp"],
    toolchains = [
        str(Label("//rust:toolchain_type")),
        "@bazel_tools//tools/cpp:toolchain_type",
    ],
    doc = dedent("""\
        Builds a Rust library crate.

        Example:

        Suppose you have the following directory structure for a simple Rust library crate:

        ```output
        [workspace]/
            WORKSPACE
            hello_lib/
                BUILD
                src/
                    greeter.rs
                    lib.rs
        ```

        `hello_lib/src/greeter.rs`:
        ```rust
        pub struct Greeter {
            greeting: String,
        }

        impl Greeter {
            pub fn new(greeting: &str) -> Greeter {
                Greeter { greeting: greeting.to_string(), }
            }

            pub fn greet(&self, thing: &str) {
                println!("{} {}", &self.greeting, thing);
            }
        }
        ```

        `hello_lib/src/lib.rs`:

        ```rust
        pub mod greeter;
        ```

        `hello_lib/BUILD`:
        ```python
        package(default_visibility = ["//visibility:public"])

        load("@rules_rust//rust:defs.bzl", "rust_library")

        rust_library(
            name = "hello_lib",
            srcs = [
                "src/greeter.rs",
                "src/lib.rs",
            ],
        )
        ```

        Build the library:
        ```output
        $ bazel build //hello_lib
        INFO: Found 1 target...
        Target //examples/rust/hello_lib:hello_lib up-to-date:
        bazel-bin/examples/rust/hello_lib/libhello_lib.rlib
        INFO: Elapsed time: 1.245s, Critical Path: 1.01s
        ```
        """),
)

def _rust_static_library_transition_impl(settings, attr):
    return {
        "//command_line_option:platforms": str(attr.platform) if attr.platform else settings["//command_line_option:platforms"],
    }

_rust_static_library_transition = transition(
    implementation = _rust_static_library_transition_impl,
    inputs = [
        "//command_line_option:platforms",
    ],
    outputs = [
        "//command_line_option:platforms",
    ],
)

rust_static_library = rule(
    implementation = _rust_static_library_impl,
    attrs = _common_attrs | {
        "platform": attr.label(
            doc = "Optional platform to transition the static library to.",
            default = None,
        ),
        "_allowlist_function_transition": attr.label(
            default = "@bazel_tools//tools/allowlists/function_transition_allowlist",
        ),
    },
    fragments = ["cpp"],
    cfg = _rust_static_library_transition,
    toolchains = [
        str(Label("//rust:toolchain_type")),
        "@bazel_tools//tools/cpp:toolchain_type",
    ],
    provides = [CcInfo],
    doc = dedent("""\
        Builds a Rust static library.

        This static library will contain all transitively reachable crates and native objects.
        It is meant to be used when producing an artifact that is then consumed by some other build system
        (for example to produce an archive that Python program links against).

        This rule provides CcInfo, so it can be used everywhere Bazel expects `rules_cc`.

        When building the whole binary in Bazel, use `rust_library` instead.
        """),
)

def _rust_shared_library_transition_impl(settings, attr):
    return {
        "//command_line_option:platforms": str(attr.platform) if attr.platform else settings["//command_line_option:platforms"],
    }

_rust_shared_library_transition = transition(
    implementation = _rust_shared_library_transition_impl,
    inputs = [
        "//command_line_option:platforms",
    ],
    outputs = [
        "//command_line_option:platforms",
    ],
)

rust_shared_library = rule(
    implementation = _rust_shared_library_impl,
    attrs = _common_attrs | _experimental_use_cc_common_link_attrs | {
        "platform": attr.label(
            doc = "Optional platform to transition the shared library to.",
            default = None,
        ),
        "_allowlist_function_transition": attr.label(
            default = "@bazel_tools//tools/allowlists/function_transition_allowlist",
        ),
        "_use_grep_includes": attr.bool(default = True),
    },
    fragments = ["cpp"],
    cfg = _rust_shared_library_transition,
    toolchains = [
        str(Label("//rust:toolchain_type")),
        "@bazel_tools//tools/cpp:toolchain_type",
    ],
    provides = [CcInfo],
    doc = dedent("""\
        Builds a Rust shared library.

        This shared library will contain all transitively reachable crates and native objects.
        It is meant to be used when producing an artifact that is then consumed by some other build system
        (for example to produce a shared library that Python program links against).

        This rule provides CcInfo, so it can be used everywhere Bazel expects `rules_cc`.

        When building the whole binary in Bazel, use `rust_library` instead.
        """),
)

def _proc_macro_dep_transition_impl(settings, _attr):
    if settings["//rust/private:is_proc_macro_dep_enabled"]:
        return {"//rust/private:is_proc_macro_dep": True}
    else:
        return []

_proc_macro_dep_transition = transition(
    inputs = ["//rust/private:is_proc_macro_dep_enabled"],
    outputs = ["//rust/private:is_proc_macro_dep"],
    implementation = _proc_macro_dep_transition_impl,
)

rust_proc_macro = rule(
    implementation = _rust_proc_macro_impl,
    provides = COMMON_PROVIDERS,
    # Start by copying the common attributes, then override the `deps` attribute
    # to apply `_proc_macro_dep_transition`. To add this transition we additionally
    # need to declare `_allowlist_function_transition`, see
    # https://docs.bazel.build/versions/main/skylark/config.html#user-defined-transitions.
    attrs = dict(
        _common_attrs.items(),
        _allowlist_function_transition = attr.label(
            default = Label("//tools/allowlists/function_transition_allowlist"),
        ),
        deps = attr.label_list(
            doc = dedent("""\
                List of other libraries to be linked to this library target.

                These can be either other `rust_library` targets or `cc_library` targets if
                linking a native library.
            """),
            cfg = _proc_macro_dep_transition,
        ),
    ),
    fragments = ["cpp"],
    toolchains = [
        str(Label("//rust:toolchain_type")),
        "@bazel_tools//tools/cpp:toolchain_type",
    ],
    doc = dedent("""\
        Builds a Rust proc-macro crate.
    """),
)

_rust_binary_attrs = {
    "binary_name": attr.string(
        doc = dedent("""\
            Override the resulting binary file name. By default, the binary file will be named using the `name` attribute on this rule,
            however sometimes that is not deseriable.
        """),
    ),
    "crate_type": attr.string(
        doc = dedent("""\
            Crate type that will be passed to `rustc` to be used for building this crate.

            This option is a temporary workaround and should be used only when building
            for WebAssembly targets (//rust/platform:wasi and //rust/platform:wasm).
        """),
        default = "bin",
    ),
    "env": attr.string_dict(
        mandatory = False,
        doc = dedent("""\
            Specifies additional environment variables to set when the target is executed by bazel run.
            Values are subject to `$(rootpath)`, `$(execpath)`, location, and
            ["Make variable"](https://docs.bazel.build/versions/master/be/make-variables.html) substitution.

            Execpath returns absolute path, and in order to be able to construct the absolute path we
            need to wrap the test binary in a launcher. Using a launcher comes with complications, such as
            more complicated debugger attachment.
        """),
    ),
    "linker_script": attr.label(
        doc = dedent("""\
            Link script to forward into linker via rustc options.
        """),
        allow_single_file = True,
    ),
    "out_binary": attr.bool(
        doc = (
            "Force a target, regardless of it's `crate_type`, to always mark the " +
            "file as executable. This attribute is only used to support wasm targets but is " +
            "expected to be removed following a resolution to https://github.com/bazelbuild/rules_rust/issues/771."
        ),
        default = False,
    ),
    "stamp": _stamp_attribute(default_value = -1),
    "_use_grep_includes": attr.bool(default = True),
} | _experimental_use_cc_common_link_attrs

def _rust_binary_transition_impl(settings, attr):
    return {
        "//command_line_option:platforms": str(attr.platform) if attr.platform else settings["//command_line_option:platforms"],
    }

_rust_binary_transition = transition(
    implementation = _rust_binary_transition_impl,
    inputs = [
        "//command_line_option:platforms",
    ],
    outputs = [
        "//command_line_option:platforms",
    ],
)

rust_binary = rule(
    implementation = _rust_binary_impl,
    provides = COMMON_PROVIDERS,
    attrs = _common_attrs | _rust_binary_attrs | {
        "platform": attr.label(
            doc = "Optional platform to transition the binary to.",
            default = None,
        ),
        "_allowlist_function_transition": attr.label(
            default = "@bazel_tools//tools/allowlists/function_transition_allowlist",
        ),
    },
    executable = True,
    fragments = ["cpp"],
    cfg = _rust_binary_transition,
    toolchains = [
        str(Label("//rust:toolchain_type")),
        "@bazel_tools//tools/cpp:toolchain_type",
    ],
    doc = dedent("""\
        Builds a Rust binary crate.

        Example:

        Suppose you have the following directory structure for a Rust project with a
        library crate, `hello_lib`, and a binary crate, `hello_world` that uses the
        `hello_lib` library:

        ```output
        [workspace]/
            WORKSPACE
            hello_lib/
                BUILD
                src/
                    lib.rs
            hello_world/
                BUILD
                src/
                    main.rs
        ```

        `hello_lib/src/lib.rs`:
        ```rust
        pub struct Greeter {
            greeting: String,
        }

        impl Greeter {
            pub fn new(greeting: &str) -> Greeter {
                Greeter { greeting: greeting.to_string(), }
            }

            pub fn greet(&self, thing: &str) {
                println!("{} {}", &self.greeting, thing);
            }
        }
        ```

        `hello_lib/BUILD`:
        ```python
        package(default_visibility = ["//visibility:public"])

        load("@rules_rust//rust:defs.bzl", "rust_library")

        rust_library(
            name = "hello_lib",
            srcs = ["src/lib.rs"],
        )
        ```

        `hello_world/src/main.rs`:
        ```rust
        extern crate hello_lib;

        fn main() {
            let hello = hello_lib::Greeter::new("Hello");
            hello.greet("world");
        }
        ```

        `hello_world/BUILD`:
        ```python
        load("@rules_rust//rust:defs.bzl", "rust_binary")

        rust_binary(
            name = "hello_world",
            srcs = ["src/main.rs"],
            deps = ["//hello_lib"],
        )
        ```

        Build and run `hello_world`:
        ```
        $ bazel run //hello_world
        INFO: Found 1 target...
        Target //examples/rust/hello_world:hello_world up-to-date:
        bazel-bin/examples/rust/hello_world/hello_world
        INFO: Elapsed time: 1.308s, Critical Path: 1.22s

        INFO: Running command line: bazel-bin/examples/rust/hello_world/hello_world
        Hello world
        ```

        On Windows, a PDB file containing debugging information is available under
        the key `pdb_file` in `OutputGroupInfo`. Similarly on macOS, a dSYM folder
        is available under the key `dsym_folder` in `OutputGroupInfo`.
"""),
)

def _common_attrs_for_binary_without_process_wrapper(attrs):
    new_attr = dict(attrs)

    # use a fake process wrapper
    new_attr["_process_wrapper"] = attr.label(
        default = None,
        executable = True,
        allow_single_file = True,
        cfg = "exec",
    )

    new_attr["_bootstrap_process_wrapper"] = attr.label(
        default = Label("//util/process_wrapper:bootstrap_process_wrapper"),
        executable = True,
        allow_single_file = True,
        cfg = "exec",
    )

    # fix stamp = 0
    new_attr["stamp"] = attr.int(
        doc = dedent("""\
            Fix `stamp = 0` as stamping is not supported when building without process_wrapper:
            https://github.com/bazelbuild/rules_rust/blob/8df4517d370b0c543a01ba38b63e1d5a4104b035/rust/private/rustc.bzl#L955
        """),
        default = 0,
        values = [0],
    )

    return new_attr

# Provides an internal rust_{binary,library} to use that we can use to build the process
# wrapper, this breaks the dependency of rust_* on the process wrapper by
# setting it to None, which the functions in rustc detect and build accordingly.
rust_binary_without_process_wrapper = rule(
    implementation = _rust_binary_impl,
    provides = COMMON_PROVIDERS,
    attrs = _common_attrs_for_binary_without_process_wrapper(_common_attrs | _rust_binary_attrs | {
        "platform": attr.label(
            doc = "Optional platform to transition the binary to.",
            default = None,
        ),
        "_allowlist_function_transition": attr.label(
            default = "@bazel_tools//tools/allowlists/function_transition_allowlist",
        ),
    }),
    executable = True,
    fragments = ["cpp"],
    cfg = _rust_binary_transition,
    toolchains = [
        str(Label("//rust:toolchain_type")),
        "@bazel_tools//tools/cpp:toolchain_type",
    ],
)

rust_library_without_process_wrapper = rule(
    implementation = _rust_library_impl,
    provides = COMMON_PROVIDERS,
    attrs = dict(_common_attrs_for_binary_without_process_wrapper(_common_attrs).items()),
    fragments = ["cpp"],
    toolchains = [
        str(Label("//rust:toolchain_type")),
        "@bazel_tools//tools/cpp:toolchain_type",
    ],
)

def _rust_test_transition_impl(settings, attr):
    return {
        "//command_line_option:platforms": str(attr.platform) if attr.platform else settings["//command_line_option:platforms"],
    }

_rust_test_transition = transition(
    implementation = _rust_test_transition_impl,
    inputs = [
        "//command_line_option:platforms",
    ],
    outputs = [
        "//command_line_option:platforms",
    ],
)

rust_test = rule(
    implementation = _rust_test_impl,
    provides = COMMON_PROVIDERS,
    attrs = _common_attrs | _rust_test_attrs | {
        "platform": attr.label(
            doc = "Optional platform to transition the test to.",
            default = None,
        ),
        "_allowlist_function_transition": attr.label(
            default = "@bazel_tools//tools/allowlists/function_transition_allowlist",
        ),
    },
    executable = True,
    fragments = ["cpp"],
    cfg = _rust_test_transition,
    test = True,
    toolchains = [
        str(Label("//rust:toolchain_type")),
        "@bazel_tools//tools/cpp:toolchain_type",
    ],
    doc = dedent("""\
        Builds a Rust test crate.

        Examples:

        Suppose you have the following directory structure for a Rust library crate \
        with unit test code in the library sources:

        ```output
        [workspace]/
            WORKSPACE
            hello_lib/
                BUILD
                src/
                    lib.rs
        ```

        `hello_lib/src/lib.rs`:
        ```rust
        pub struct Greeter {
            greeting: String,
        }

        impl Greeter {
            pub fn new(greeting: &str) -> Greeter {
                Greeter { greeting: greeting.to_string(), }
            }

            pub fn greet(&self, thing: &str) -> String {
                format!("{} {}", &self.greeting, thing)
            }
        }

        #[cfg(test)]
        mod test {
            use super::Greeter;

            #[test]
            fn test_greeting() {
                let hello = Greeter::new("Hi");
                assert_eq!("Hi Rust", hello.greet("Rust"));
            }
        }
        ```

        To build and run the tests, simply add a `rust_test` rule with no `srcs`
        and only depends on the `hello_lib` `rust_library` target via the
        `crate` attribute:

        `hello_lib/BUILD`:
        ```python
        load("@rules_rust//rust:defs.bzl", "rust_library", "rust_test")

        rust_library(
            name = "hello_lib",
            srcs = ["src/lib.rs"],
        )

        rust_test(
            name = "hello_lib_test",
            crate = ":hello_lib",
            # You may add other deps that are specific to the test configuration
            deps = ["//some/dev/dep"],
        )
        ```

        Run the test with `bazel test //hello_lib:hello_lib_test`.

        ### Example: `test` directory

        Integration tests that live in the [`tests` directory][int-tests], they are \
        essentially built as separate crates. Suppose you have the following directory \
        structure where `greeting.rs` is an integration test for the `hello_lib` \
        library crate:

        [int-tests]: http://doc.rust-lang.org/book/testing.html#the-tests-directory

        ```output
        [workspace]/
            WORKSPACE
            hello_lib/
                BUILD
                src/
                    lib.rs
                tests/
                    greeting.rs
        ```

        `hello_lib/tests/greeting.rs`:
        ```rust
        extern crate hello_lib;

        use hello_lib;

        #[test]
        fn test_greeting() {
            let hello = greeter::Greeter::new("Hello");
            assert_eq!("Hello world", hello.greeting("world"));
        }
        ```

        To build the `greeting.rs` integration test, simply add a `rust_test` target
        with `greeting.rs` in `srcs` and a dependency on the `hello_lib` target:

        `hello_lib/BUILD`:
        ```python
        load("@rules_rust//rust:defs.bzl", "rust_library", "rust_test")

        rust_library(
            name = "hello_lib",
            srcs = ["src/lib.rs"],
        )

        rust_test(
            name = "greeting_test",
            srcs = ["tests/greeting.rs"],
            deps = [":hello_lib"],
        )
        ```

        Run the test with `bazel test //hello_lib:greeting_test`.
"""),
)

def rust_test_suite(name, srcs, shared_srcs = [], **kwargs):
    """A rule for creating a test suite for a set of `rust_test` targets.

    This rule can be used for setting up typical rust [integration tests][it]. Given the following
    directory structure:

    ```text
    [crate]/
        BUILD.bazel
        src/
            lib.rs
            main.rs
        tests/
            integrated_test_a.rs
            integrated_test_b.rs
            integrated_test_c.rs
            patterns/
                fibonacci_test.rs
            helpers/
                mod.rs
    ```

    The rule can be used to generate [rust_test](#rust_test) targets for each source file under `tests`
    and a [test_suite][ts] which encapsulates all tests.

    ```python
    load("//rust:defs.bzl", "rust_binary", "rust_library", "rust_test_suite")

    rust_library(
        name = "math_lib",
        srcs = ["src/lib.rs"],
    )

    rust_binary(
        name = "math_bin",
        srcs = ["src/main.rs"],
    )

    rust_test_suite(
        name = "integrated_tests_suite",
        srcs = glob(["tests/**"]),
        shared_srcs=glob(["tests/helpers/**"]),
        deps = [":math_lib"],
    )
    ```

    [it]: https://doc.rust-lang.org/rust-by-example/testing/integration_testing.html
    [ts]: https://docs.bazel.build/versions/master/be/general.html#test_suite

    Args:
        name (str): The name of the `test_suite`.
        srcs (list): All test sources, typically `glob(["tests/**/*.rs"])`.
        shared_srcs (list): Optional argument for sources shared among tests, typically helper functions.
        **kwargs (dict): Additional keyword arguments for the underyling [rust_test](#rust_test) targets. The
            `tags` argument is also passed to the generated `test_suite` target.
    """
    tests = []

    # If test_suite.tests is empty, Bazel will unhelpfully include all tests
    # from the package. Require an extra tag so they are filtered out again.
    tags = ["restrict_" + name] + kwargs.pop("tags", [])

    for src in srcs:
        if not src.endswith(".rs"):
            fail("srcs should have `.rs` extensions")

        if src in shared_srcs:
            continue

        # Prefixed with `name` to allow parameterization with macros
        # The test name should not end with `.rs`
        test_name = name + "_" + src[:-3]
        rust_test(
            name = test_name,
            crate_root = src,
            srcs = [src] + shared_srcs,
            tags = tags,
            **kwargs
        )
        tests.append(test_name)

    native.test_suite(
        name = name,
        tests = tests,
        tags = tags,
    )

rust_library_group = rule(
    implementation = _rust_library_group_impl,
    provides = [rust_common.crate_group_info],
    attrs = {
        "deps": attr.label_list(
            doc = "Other dependencies to forward through this crate group.",
            providers = [[rust_common.crate_group_info], [rust_common.crate_info]],
        ),
    },
    doc = dedent("""\
        Functions as an alias for a set of dependencies.

        Specifically, the following are equivalent:

        ```starlark
        rust_library_group(
            name = "crate_group",
            deps = [
                ":crate1",
                ":crate2",
            ],
        )

        rust_library(
            name = "foobar",
            deps = [":crate_group"],
            ...
        )
        ```

        and

        ```starlark
        rust_library(
            name = "foobar",
            deps = [
                ":crate1",
                ":crate2",
            ],
            ...
        )
        ```
    """),
)
