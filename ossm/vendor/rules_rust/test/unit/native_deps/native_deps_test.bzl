"""Unittests for rust rules."""

load("@bazel_skylib//lib:unittest.bzl", "analysistest", "asserts")
load("@rules_cc//cc:defs.bzl", "cc_library")
load("//rust:defs.bzl", "rust_binary", "rust_library", "rust_proc_macro", "rust_shared_library", "rust_static_library")
load(
    "//test/unit:common.bzl",
    "assert_argv_contains",
    "assert_argv_contains_not",
    "assert_argv_contains_prefix",
    "assert_argv_contains_prefix_not",
    "assert_argv_contains_prefix_suffix",
    "assert_list_contains_adjacent_elements",
)

def _get_toolchain(ctx):
    return ctx.attr._toolchain[platform_common.ToolchainInfo]

def _get_bin_dir_from_action(action):
    """Extract the bin directory from an action's outputs.

    This handles config transitions that add suffixes like -ST-<hash>.

    Args:
        action: The action to extract the bin directory from.

    Returns:
        The bin directory path as a string.
    """
    bin_dir = action.outputs.to_list()[0].dirname
    if "/bin/" in bin_dir:
        bin_dir = bin_dir.split("/bin/")[0] + "/bin"
    return bin_dir

def _get_darwin_component(arg):
    """Extract darwin component from a path.

    Args:
        arg: Path like "path/to/darwin_x86_64-fastbuild-ST-abc123/package"

    Returns:
        Darwin component like "darwin" (ignoring arch and compilation mode)
    """

    # path/to/darwin_x86_64-fastbuild-ST-abc123/package -> darwin_x86_64-fastbuild-ST-abc123
    darwin_component = [x for x in arg.split("/") if x.startswith("darwin")][0]

    # darwin_x86_64-fastbuild-ST-abc123 -> darwin
    return darwin_component.split("-")[0]

def _assert_bin_dir_structure(env, ctx, bin_dir, toolchain):
    """Validate bin_dir structure, ignoring ST-{hash} suffix from config transitions.

    Args:
        env: The analysis test environment
        ctx: The test context
        bin_dir: The bin directory path to validate
        toolchain: The toolchain info
    """
    compilation_mode = ctx.var["COMPILATION_MODE"]

    # bin_dir should be like: bazel-out/{platform}-{mode}[-ST-{hash}]/bin
    asserts.true(env, bin_dir.startswith("bazel-out/"), "bin_dir should start with bazel-out/")
    asserts.true(env, bin_dir.endswith("/bin"), "bin_dir should end with /bin")

    # Validate it contains compilation mode (ignoring potential ST-{hash})
    bin_dir_components = bin_dir.split("/")[1]  # Get the platform-mode component
    asserts.true(
        env,
        compilation_mode in bin_dir_components,
        "bin_dir should contain compilation mode: expected '{}' in '{}'".format(compilation_mode, bin_dir_components),
    )

    # For Darwin platforms, validate darwin component
    if toolchain.target_os in ["macos", "darwin"] and "darwin" in bin_dir:
        darwin_component = _get_darwin_component(bin_dir)
        asserts.true(
            env,
            darwin_component.startswith("darwin"),
            "darwin component should start with 'darwin', got '{}'".format(darwin_component),
        )

def _rlib_has_no_native_libs_test_impl(ctx):
    env = analysistest.begin(ctx)
    tut = analysistest.target_under_test(env)
    action = tut.actions[0]
    assert_argv_contains(env, action, "--crate-type=rlib")
    assert_argv_contains_not(env, action, "-lstatic=native_dep")
    assert_argv_contains_not(env, action, "-ldylib=native_dep")
    assert_argv_contains_prefix_not(env, action, "--codegen=linker=")
    return analysistest.end(env)

def _cdylib_has_native_libs_test_impl(ctx):
    env = analysistest.begin(ctx)
    tut = analysistest.target_under_test(env)
    action = tut.actions[0]
    toolchain = _get_toolchain(ctx)
    compilation_mode = ctx.var["COMPILATION_MODE"]
    pic_suffix = _get_pic_suffix(ctx, compilation_mode)
    assert_argv_contains_prefix_suffix(env, action, "-Lnative=", "/native_deps")
    assert_argv_contains(env, action, "--crate-type=cdylib")
    assert_argv_contains(env, action, "-lstatic=native_dep{}".format(pic_suffix))
    if toolchain.target_os == "windows":
        if toolchain.target_triple.abi == "msvc":
            native_link_arg = "-Clink-arg=native_dep.lib"
        else:
            native_link_arg = "-Clink-arg=-lnative_dep.lib"
    else:
        native_link_arg = "-Clink-arg=-lnative_dep{}".format(pic_suffix)
    assert_argv_contains(env, action, native_link_arg)
    assert_argv_contains_prefix(env, action, "--codegen=linker=")
    return analysistest.end(env)

def _staticlib_has_native_libs_test_impl(ctx):
    env = analysistest.begin(ctx)
    tut = analysistest.target_under_test(env)
    action = tut.actions[0]
    toolchain = _get_toolchain(ctx)
    assert_argv_contains_prefix_suffix(env, action, "-Lnative=", "/native_deps")
    assert_argv_contains(env, action, "--crate-type=staticlib")
    assert_argv_contains(env, action, "-lstatic=native_dep")
    if toolchain.target_os == "windows":
        if toolchain.target_triple.abi == "msvc":
            native_link_arg = "-Clink-arg=native_dep.lib"
        else:
            native_link_arg = "-Clink-arg=-lnative_dep.lib"
    else:
        native_link_arg = "-Clink-arg=-lnative_dep"
    assert_argv_contains(env, action, native_link_arg)
    assert_argv_contains_prefix(env, action, "--codegen=linker=")
    return analysistest.end(env)

def _proc_macro_has_native_libs_test_impl(ctx):
    env = analysistest.begin(ctx)
    tut = analysistest.target_under_test(env)
    action = tut.actions[0]
    toolchain = _get_toolchain(ctx)
    compilation_mode = ctx.var["COMPILATION_MODE"]
    pic_suffix = _get_pic_suffix(ctx, compilation_mode)
    assert_argv_contains_prefix_suffix(env, action, "-Lnative=", "/native_deps")
    assert_argv_contains(env, action, "--crate-type=proc-macro")
    assert_argv_contains(env, action, "-lstatic=native_dep{}".format(pic_suffix))
    if toolchain.target_os == "windows":
        if toolchain.target_triple.abi == "msvc":
            native_link_arg = "-Clink-arg=native_dep.lib"
        else:
            native_link_arg = "-Clink-arg=-lnative_dep.lib"
    else:
        native_link_arg = "-Clink-arg=-lnative_dep{}".format(pic_suffix)
    assert_argv_contains(env, action, native_link_arg)
    assert_argv_contains_prefix(env, action, "--codegen=linker=")
    return analysistest.end(env)

def _bin_has_native_libs_test_impl(ctx):
    env = analysistest.begin(ctx)
    tut = analysistest.target_under_test(env)
    action = tut.actions[0]
    toolchain = _get_toolchain(ctx)
    assert_argv_contains_prefix_suffix(env, action, "-Lnative=", "/native_deps")
    assert_argv_contains(env, action, "-lstatic=native_dep")
    if toolchain.target_os == "windows":
        if toolchain.target_triple.abi == "msvc":
            native_link_arg = "-Clink-arg=native_dep.lib"
        else:
            native_link_arg = "-Clink-arg=-lnative_dep.lib"
    else:
        native_link_arg = "-Clink-arg=-lnative_dep"
    assert_argv_contains(env, action, native_link_arg)
    assert_argv_contains_prefix(env, action, "--codegen=linker=")
    return analysistest.end(env)

def _extract_linker_args(argv):
    return [
        a.removeprefix("--codegen=").removeprefix("-C").removeprefix("link-arg=").removeprefix("link-args=")
        for a in argv
        if (
            a.startswith("--codegen=link-arg=") or
            a.startswith("--codegen=link-args=") or
            a.startswith("-Clink-args=") or
            a.startswith("-Clink-arg=") or
            a.startswith("link-args=") or
            a.startswith("link-arg=") or
            a.startswith("-l") or
            a.endswith(".lo") or
            a.endswith(".o")
        )
    ]

def _bin_has_native_dep_and_alwayslink_test_impl(ctx, use_cc_linker):
    env = analysistest.begin(ctx)
    tut = analysistest.target_under_test(env)
    action = tut.actions[0]

    toolchain = _get_toolchain(ctx)
    link_args = _extract_linker_args(action.argv)
    bin_dir = _get_bin_dir_from_action(action)

    # Validate bin_dir structure (ignoring ST-{hash} suffix from config transitions)
    _assert_bin_dir_structure(env, ctx, bin_dir, toolchain)

    if toolchain.target_os in ["macos", "darwin"]:
        if use_cc_linker:
            # When using CC linker, args are passed with -Wl, prefix as separate arguments
            want = [
                "-lstatic=native_dep",
                "-lnative_dep",
                "-Wl,-force_load",
                "-Wl,{}/test/unit/native_deps/libalwayslink.lo".format(bin_dir),
            ]
        else:
            # When using rust-lld directly, args are passed without prefix as separate arguments
            want = [
                "-lstatic=native_dep",
                "-lnative_dep",
                "-force_load",
                "{}/test/unit/native_deps/libalwayslink.lo".format(bin_dir),
            ]
        assert_list_contains_adjacent_elements(env, link_args, want)
    elif toolchain.target_os == "windows":
        if toolchain.target_triple.abi == "msvc":
            want = [
                "-lstatic=native_dep",
                "native_dep.lib",
                "/WHOLEARCHIVE:{}/test/unit/native_deps/alwayslink.lo.lib".format(bin_dir),
            ]
        elif use_cc_linker:
            want = [
                "-lstatic=native_dep",
                "native_dep.lib",
                "-Wl,--whole-archive",
                "{}/test/unit/native_deps/alwayslink.lo.lib".format(bin_dir),
                "-Wl,--no-whole-archive",
            ]
        else:
            want = [
                "-lstatic=native_dep",
                "native_dep.lib",
                "--whole-archive",
                "{}/test/unit/native_deps/alwayslink.lo.lib".format(bin_dir),
                "--no-whole-archive",
            ]
    elif toolchain.target_arch == "s390x":
        want = [
            "-lstatic=native_dep",
            "link-arg=-Wl,--whole-archive",
            "link-arg={}/test/unit/native_deps/libalwayslink.lo".format(bin_dir),
            "link-arg=-Wl,--no-whole-archive",
        ]
    elif use_cc_linker:
        want = [
            "-lstatic=native_dep",
            "-lnative_dep",
            "-Wl,--whole-archive",
            "{}/test/unit/native_deps/libalwayslink.lo".format(bin_dir),
            "-Wl,--no-whole-archive",
        ]
    else:
        want = [
            "-lstatic=native_dep",
            "-lnative_dep",
            "--whole-archive",
            "{}/test/unit/native_deps/libalwayslink.lo".format(bin_dir),
            "--no-whole-archive",
        ]
    assert_list_contains_adjacent_elements(env, link_args, want)
    return analysistest.end(env)

def _cdylib_has_native_dep_and_alwayslink_test_impl(ctx, use_cc_linker):
    toolchain = _get_toolchain(ctx)

    env = analysistest.begin(ctx)
    tut = analysistest.target_under_test(env)
    action = tut.actions[0]

    linker_args = _extract_linker_args(action.argv)
    bin_dir = _get_bin_dir_from_action(action)

    # Validate bin_dir structure (ignoring ST-{hash} suffix from config transitions)
    _assert_bin_dir_structure(env, ctx, bin_dir, toolchain)

    compilation_mode = ctx.var["COMPILATION_MODE"]
    pic_suffix = _get_pic_suffix(ctx, compilation_mode)

    if toolchain.target_os in ["macos", "darwin"]:
        if use_cc_linker:
            # When using CC linker, args are passed with -Wl, prefix as separate arguments
            want = [
                "-lstatic=native_dep{}".format(pic_suffix),
                "-lnative_dep{}".format(pic_suffix),
                "-Wl,-force_load",
                "-Wl,{}/test/unit/native_deps/libalwayslink{}.lo".format(bin_dir, pic_suffix),
            ]
        else:
            # When using rust-lld directly, args are passed without prefix as separate arguments
            want = [
                "-lstatic=native_dep{}".format(pic_suffix),
                "-lnative_dep{}".format(pic_suffix),
                "-force_load",
                "{}/test/unit/native_deps/libalwayslink{}.lo".format(bin_dir, pic_suffix),
            ]
    elif toolchain.target_os == "windows":
        if toolchain.target_triple.abi == "msvc":
            want = [
                "-lstatic=native_dep",
                "native_dep.lib",
                "/WHOLEARCHIVE:{}/test/unit/native_deps/alwayslink.lo.lib".format(bin_dir),
            ]
        elif use_cc_linker:
            want = [
                "-lstatic=native_dep",
                "native_dep.lib",
                "-Wl,--whole-archive",
                "{}/test/unit/native_deps/alwayslink.lo.lib".format(bin_dir),
                "-Wl,--no-whole-archive",
            ]
        else:
            want = [
                "-lstatic=native_dep",
                "native_dep.lib",
                "--whole-archive",
                "{}/test/unit/native_deps/alwayslink.lo.lib".format(bin_dir),
                "--no-whole-archive",
            ]
    elif toolchain.target_arch == "s390x":
        want = [
            "-lstatic=native_dep{}".format(pic_suffix),
            "link-arg=-Wl,--whole-archive",
            "link-arg={}/test/unit/native_deps/libalwayslink{}.lo".format(bin_dir, pic_suffix),
            "link-arg=-Wl,--no-whole-archive",
        ]
    elif use_cc_linker:
        # CC linker uses -Wl, prefix but arguments are separate
        want = [
            "-lstatic=native_dep{}".format(pic_suffix),
            "-lnative_dep{}".format(pic_suffix),
            "-Wl,--whole-archive",
            "{}/test/unit/native_deps/libalwayslink{}.lo".format(bin_dir, pic_suffix),
            "-Wl,--no-whole-archive",
        ]
    else:
        # rust-lld doesn't use -Wl, prefix, so flags and path are separate
        want = [
            "-lstatic=native_dep{}".format(pic_suffix),
            "-lnative_dep{}".format(pic_suffix),
            "--whole-archive",
            "{}/test/unit/native_deps/libalwayslink{}.lo".format(bin_dir, pic_suffix),
            "--no-whole-archive",
        ]
    assert_list_contains_adjacent_elements(env, linker_args, want)
    return analysistest.end(env)

def _get_pic_suffix(ctx, compilation_mode):
    toolchain = _get_toolchain(ctx)
    if toolchain.target_os in ["darwin", "macos", "windows"]:
        return ""
    return ".pic" if compilation_mode == "opt" else ""

rlib_has_no_native_libs_test = analysistest.make(_rlib_has_no_native_libs_test_impl)
staticlib_has_native_libs_test = analysistest.make(_staticlib_has_native_libs_test_impl, attrs = {
    "_toolchain": attr.label(default = Label("//rust/toolchain:current_rust_toolchain")),
})
cdylib_has_native_libs_test = analysistest.make(_cdylib_has_native_libs_test_impl, attrs = {
    "_toolchain": attr.label(default = Label("//rust/toolchain:current_rust_toolchain")),
})
proc_macro_has_native_libs_test = analysistest.make(_proc_macro_has_native_libs_test_impl, attrs = {
    "_toolchain": attr.label(default = Label("//rust/toolchain:current_rust_toolchain")),
})
bin_has_native_libs_test = analysistest.make(_bin_has_native_libs_test_impl, attrs = {
    "_linker_preference": attr.label(default = Label("//rust/settings:toolchain_linker_preference")),
    "_toolchain": attr.label(default = Label("//rust/toolchain:current_rust_toolchain")),
})

def _bin_has_native_dep_and_alwayslink_rust_linker_test_impl(ctx):
    return _bin_has_native_dep_and_alwayslink_test_impl(ctx, False)

bin_has_native_dep_and_alwayslink_rust_linker_test = analysistest.make(
    _bin_has_native_dep_and_alwayslink_rust_linker_test_impl,
    attrs = {
        "_linker_preference": attr.label(default = Label("//rust/settings:toolchain_linker_preference")),
        "_toolchain": attr.label(default = Label("//rust/toolchain:current_rust_toolchain")),
    },
    config_settings = {
        str(Label("//rust/settings:toolchain_linker_preference")): "rust",
        # Coverage is not supported on musl so it should be disabled for these targets.
        "//command_line_option:collect_code_coverage": False,
    },
)

def _bin_has_native_dep_and_alwayslink_cc_linker_test_impl(ctx):
    return _bin_has_native_dep_and_alwayslink_test_impl(ctx, True)

bin_has_native_dep_and_alwayslink_cc_linker_test = analysistest.make(
    _bin_has_native_dep_and_alwayslink_cc_linker_test_impl,
    attrs = {
        "_linker_preference": attr.label(default = Label("//rust/settings:toolchain_linker_preference")),
        "_toolchain": attr.label(default = Label("//rust/toolchain:current_rust_toolchain")),
    },
    config_settings = {
        str(Label("//rust/settings:toolchain_linker_preference")): "cc",
    },
)

def _cdylib_has_native_dep_and_alwayslink_rust_linker_test_impl(ctx):
    return _cdylib_has_native_dep_and_alwayslink_test_impl(ctx, False)

cdylib_has_native_dep_and_alwayslink_rust_linker_test = analysistest.make(
    _cdylib_has_native_dep_and_alwayslink_rust_linker_test_impl,
    attrs = {
        "_linker_preference": attr.label(default = Label("//rust/settings:toolchain_linker_preference")),
        "_toolchain": attr.label(default = Label("//rust/toolchain:current_rust_toolchain")),
    },
    config_settings = {
        str(Label("//rust/settings:toolchain_linker_preference")): "rust",
        "//command_line_option:collect_code_coverage": False,
    },
)

def _cdylib_has_native_dep_and_alwayslink_cc_linker_test_impl(ctx):
    return _cdylib_has_native_dep_and_alwayslink_test_impl(ctx, True)

cdylib_has_native_dep_and_alwayslink_cc_linker_test = analysistest.make(
    _cdylib_has_native_dep_and_alwayslink_cc_linker_test_impl,
    attrs = {
        "_linker_preference": attr.label(default = Label("//rust/settings:toolchain_linker_preference")),
        "_toolchain": attr.label(default = Label("//rust/toolchain:current_rust_toolchain")),
    },
    config_settings = {
        str(Label("//rust/settings:toolchain_linker_preference")): "cc",
    },
)

def _native_dep_test():
    rust_library(
        name = "rlib_has_no_native_dep",
        srcs = ["lib_using_native_dep.rs"],
        edition = "2018",
        deps = [":native_dep"],
    )

    rust_static_library(
        name = "staticlib_has_native_dep",
        srcs = ["lib_using_native_dep.rs"],
        edition = "2018",
        deps = [":native_dep"],
    )

    rust_shared_library(
        name = "cdylib_has_native_dep",
        srcs = ["lib_using_native_dep.rs"],
        edition = "2018",
        deps = [":native_dep"],
    )

    rust_proc_macro(
        name = "proc_macro_has_native_dep",
        srcs = ["proc_macro_using_native_dep.rs"],
        edition = "2018",
        deps = [":native_dep"],
    )

    rust_binary(
        name = "bin_has_native_dep",
        srcs = ["bin_using_native_dep.rs"],
        edition = "2018",
        deps = [":native_dep"],
    )

    rust_binary(
        name = "bin_has_native_dep_and_alwayslink",
        srcs = ["bin_using_native_dep.rs"],
        edition = "2018",
        deps = [":native_dep", ":alwayslink"],
    )

    cc_library(
        name = "native_dep",
        srcs = ["native_dep.cc"],
        visibility = ["//test/unit:__subpackages__"],
    )

    cc_library(
        name = "alwayslink",
        srcs = ["alwayslink.cc"],
        alwayslink = 1,
    )

    rust_shared_library(
        name = "cdylib_has_native_dep_and_alwayslink",
        srcs = ["lib_using_native_dep.rs"],
        edition = "2018",
        deps = [":native_dep", ":alwayslink"],
    )

    # Note: cdylib tests use the same target but we'll force the setting via config_setting
    # The test will check the _linker_preference build setting to determine expected behavior

    rlib_has_no_native_libs_test(
        name = "rlib_has_no_native_libs_test",
        target_under_test = ":rlib_has_no_native_dep",
    )
    staticlib_has_native_libs_test(
        name = "staticlib_has_native_libs_test",
        target_under_test = ":staticlib_has_native_dep",
    )
    cdylib_has_native_libs_test(
        name = "cdylib_has_native_libs_test",
        target_under_test = ":cdylib_has_native_dep",
    )
    proc_macro_has_native_libs_test(
        name = "proc_macro_has_native_libs_test",
        target_under_test = ":proc_macro_has_native_dep",
    )
    bin_has_native_libs_test(
        name = "bin_has_native_libs_test",
        target_under_test = ":bin_has_native_dep",
    )
    bin_has_native_dep_and_alwayslink_rust_linker_test(
        name = "bin_has_native_dep_and_alwayslink_rust_linker_test",
        target_under_test = ":bin_has_native_dep_and_alwayslink",
        # TODO: https://github.com/bazelbuild/rules_rust/issues/390
        # Without a cc toolchain static rust is unable to find gnu
        # libraries. A favorable alternative would be to use musl
        # but there are no constraints to define this.
        target_compatible_with = select({
            "@platforms//os:linux": ["@platforms//:incompatible"],
            "//conditions:default": [],
        }),
    )
    bin_has_native_dep_and_alwayslink_cc_linker_test(
        name = "bin_has_native_dep_and_alwayslink_cc_linker_test",
        target_under_test = ":bin_has_native_dep_and_alwayslink",
    )
    cdylib_has_native_dep_and_alwayslink_rust_linker_test(
        name = "cdylib_has_native_dep_and_alwayslink_rust_linker_test",
        target_under_test = ":cdylib_has_native_dep_and_alwayslink",
        # TODO: https://github.com/bazelbuild/rules_rust/issues/390
        # Without a cc toolchain static rust is unable to find gnu
        # libraries. A favorable alternative would be to use musl
        # but there are no constraints to define this.
        target_compatible_with = select({
            "@platforms//os:linux": ["@platforms//:incompatible"],
            "//conditions:default": [],
        }),
    )
    cdylib_has_native_dep_and_alwayslink_cc_linker_test(
        name = "cdylib_has_native_dep_and_alwayslink_cc_linker_test",
        target_under_test = ":cdylib_has_native_dep_and_alwayslink",
    )

def _linkopts_propagate_test_impl(ctx):
    env = analysistest.begin(ctx)
    tut = analysistest.target_under_test(env)
    action = tut.actions[0]

    # Ensure linkopts from direct (-Llinkoptdep1) and transitive
    # (-Llinkoptdep2) dependencies are propagated.
    # Consistently with cc rules, dependency linkopts take precedence over
    # dependent linkopts (i.e. dependency linkopts appear later in the command
    # line).

    linkopt_args = _extract_linker_args(action.argv)
    assert_list_contains_adjacent_elements(
        env,
        linkopt_args,
        ["-Llinkoptdep1", "-Llinkoptdep2"],
    )
    return analysistest.end(env)

linkopts_propagate_test = analysistest.make(_linkopts_propagate_test_impl)

def _linkopts_test():
    rust_binary(
        name = "linkopts_rust_bin",
        srcs = ["bin_using_native_dep.rs"],
        edition = "2018",
        deps = [":linkopts_native_dep_a"],
    )

    cc_library(
        name = "linkopts_native_dep_a",
        srcs = ["native_dep.cc"],
        linkopts = ["-Llinkoptdep1"],
        deps = [":linkopts_native_dep_b"],
    )

    cc_library(
        name = "linkopts_native_dep_b",
        linkopts = ["-Llinkoptdep2"],
    )

    linkopts_propagate_test(
        name = "native_linkopts_propagate_test",
        target_under_test = ":linkopts_rust_bin",
    )

def _check_additional_deps_test_impl(ctx, expect_additional_deps):
    env = analysistest.begin(ctx)
    tut = analysistest.target_under_test(env)
    action = tut.actions[0]
    additional_inputs = [inp.basename for inp in action.inputs.to_list()]
    asserts.equals(env, "dynamic.lds" in additional_inputs, expect_additional_deps)
    return analysistest.end(env)

def _has_additional_deps_test_impl(ctx):
    return _check_additional_deps_test_impl(ctx, expect_additional_deps = True)

def _has_no_additional_deps_test_impl(ctx):
    return _check_additional_deps_test_impl(ctx, expect_additional_deps = False)

has_additional_deps_test = analysistest.make(_has_additional_deps_test_impl)
has_no_additional_deps_test = analysistest.make(_has_no_additional_deps_test_impl)

def _additional_deps_test():
    rust_binary(
        name = "bin_additional_deps",
        srcs = ["bin_using_native_dep.rs"],
        edition = "2018",
        deps = [":additional_deps_cc"],
    )

    rust_shared_library(
        name = "cdylib_additional_deps",
        srcs = ["lib_using_native_dep.rs"],
        edition = "2018",
        deps = [":additional_deps_cc"],
    )

    rust_library(
        name = "lib_additional_deps",
        srcs = ["lib_using_native_dep.rs"],
        edition = "2018",
        deps = ["additional_deps_cc"],
    )

    cc_library(
        name = "additional_deps_cc",
        srcs = ["native_dep.cc"],
        linkopts = ["-L$(execpath :dynamic.lds)"],
        deps = [":dynamic.lds"],
    )

    has_additional_deps_test(
        name = "bin_has_additional_deps_test",
        target_under_test = ":bin_additional_deps",
    )

    has_additional_deps_test(
        name = "cdylib_has_additional_deps_test",
        target_under_test = ":cdylib_additional_deps",
    )

    has_no_additional_deps_test(
        name = "lib_has_no_additional_deps_test",
        target_under_test = ":lib_additional_deps",
    )

def native_deps_test_suite(name):
    """Entry-point macro called from the BUILD file.

    Args:
        name: Name of the macro.
    """
    _native_dep_test()
    _linkopts_test()
    _additional_deps_test()

    native.test_suite(
        name = name,
        tests = [
            ":bin_has_additional_deps_test",
            ":bin_has_native_dep_and_alwayslink_rust_linker_test",
            ":bin_has_native_dep_and_alwayslink_cc_linker_test",
            ":bin_has_native_libs_test",
            ":cdylib_has_additional_deps_test",
            ":cdylib_has_native_dep_and_alwayslink_rust_linker_test",
            ":cdylib_has_native_dep_and_alwayslink_cc_linker_test",
            ":cdylib_has_native_libs_test",
            ":lib_has_no_additional_deps_test",
            ":native_linkopts_propagate_test",
            ":proc_macro_has_native_libs_test",
            ":rlib_has_no_native_libs_test",
            ":staticlib_has_native_libs_test",
        ],
    )
