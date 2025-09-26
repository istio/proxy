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

def _get_darwin_component(arg):
    # path/to/darwin_x86_64-fastbuild-fastbuild/package -> darwin_x86_64-fastbuild
    darwin_component = [x for x in arg.split("/") if x.startswith("darwin")][0]

    # darwin_x86_64-fastbuild -> darwin
    return darwin_component.split("-")[0]

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

def _get_workspace_prefix(ctx):
    return "" if ctx.workspace_name in ["rules_rust", "_main"] else "external/rules_rust/"

def _bin_has_native_dep_and_alwayslink_test_impl(ctx):
    env = analysistest.begin(ctx)
    tut = analysistest.target_under_test(env)
    action = tut.actions[0]

    toolchain = _get_toolchain(ctx)
    compilation_mode = ctx.var["COMPILATION_MODE"]
    workspace_prefix = _get_workspace_prefix(ctx)
    link_args = _extract_linker_args(action.argv)
    if toolchain.target_os == "darwin":
        darwin_component = _get_darwin_component(link_args[-1])
        want = [
            "-lstatic=native_dep",
            "-lnative_dep",
            "-Wl,-force_load,bazel-out/{}-{}/bin/{}test/unit/native_deps/libalwayslink.lo".format(darwin_component, compilation_mode, workspace_prefix),
        ]
        assert_list_contains_adjacent_elements(env, link_args, want)
    elif toolchain.target_os == "windows":
        if toolchain.target_triple.abi == "msvc":
            want = [
                "-lstatic=native_dep",
                "native_dep.lib",
                "/WHOLEARCHIVE:bazel-out/x64_windows-{}/bin/{}test/unit/native_deps/alwayslink.lo.lib".format(compilation_mode, workspace_prefix),
            ]
        else:
            want = [
                "-lstatic=native_dep",
                "native_dep.lib",
                "-Wl,--whole-archive",
                "bazel-out/x64_windows-{}/bin/{}test/unit/native_deps/alwayslink.lo.lib".format(compilation_mode, workspace_prefix),
                "-Wl,--no-whole-archive",
            ]
    elif toolchain.target_arch == "s390x":
        want = [
            "-lstatic=native_dep",
            "link-arg=-Wl,--whole-archive",
            "link-arg=bazel-out/s390x-{}/bin/{}test/unit/native_deps/libalwayslink.lo".format(compilation_mode, workspace_prefix),
            "link-arg=-Wl,--no-whole-archive",
        ]
    else:
        want = [
            "-lstatic=native_dep",
            "-lnative_dep",
            "-Wl,--whole-archive",
            "bazel-out/k8-{}/bin/{}test/unit/native_deps/libalwayslink.lo".format(compilation_mode, workspace_prefix),
            "-Wl,--no-whole-archive",
        ]
    assert_list_contains_adjacent_elements(env, link_args, want)
    return analysistest.end(env)

def _cdylib_has_native_dep_and_alwayslink_test_impl(ctx):
    toolchain = _get_toolchain(ctx)

    env = analysistest.begin(ctx)
    tut = analysistest.target_under_test(env)
    action = tut.actions[0]

    linker_args = _extract_linker_args(action.argv)

    toolchain = _get_toolchain(ctx)
    compilation_mode = ctx.var["COMPILATION_MODE"]
    workspace_prefix = _get_workspace_prefix(ctx)
    pic_suffix = _get_pic_suffix(ctx, compilation_mode)
    if toolchain.target_os == "darwin":
        darwin_component = _get_darwin_component(linker_args[-1])
        want = [
            "-lstatic=native_dep{}".format(pic_suffix),
            "-lnative_dep{}".format(pic_suffix),
            "-Wl,-force_load,bazel-out/{}-{}/bin/{}test/unit/native_deps/libalwayslink{}.lo".format(darwin_component, compilation_mode, workspace_prefix, pic_suffix),
        ]
    elif toolchain.target_os == "windows":
        if toolchain.target_triple.abi == "msvc":
            want = [
                "-lstatic=native_dep",
                "native_dep.lib",
                "/WHOLEARCHIVE:bazel-out/x64_windows-{}/bin/{}test/unit/native_deps/alwayslink.lo.lib".format(compilation_mode, workspace_prefix),
            ]
        else:
            want = [
                "-lstatic=native_dep",
                "native_dep.lib",
                "-Wl,--whole-archive",
                "bazel-out/x64_windows-{}/bin/{}test/unit/native_deps/alwayslink.lo.lib".format(compilation_mode, workspace_prefix),
                "-Wl,--no-whole-archive",
            ]
    elif toolchain.target_arch == "s390x":
        want = [
            "-lstatic=native_dep{}".format(pic_suffix),
            "link-arg=-Wl,--whole-archive",
            "link-arg=bazel-out/s390x-{}/bin/{}test/unit/native_deps/libalwayslink{}.lo".format(compilation_mode, workspace_prefix, pic_suffix),
            "link-arg=-Wl,--no-whole-archive",
        ]
    else:
        want = [
            "-lstatic=native_dep{}".format(pic_suffix),
            "-lnative_dep{}".format(pic_suffix),
            "-Wl,--whole-archive",
            "bazel-out/k8-{}/bin/{}test/unit/native_deps/libalwayslink{}.lo".format(compilation_mode, workspace_prefix, pic_suffix),
            "-Wl,--no-whole-archive",
        ]
    assert_list_contains_adjacent_elements(env, linker_args, want)
    return analysistest.end(env)

def _get_pic_suffix(ctx, compilation_mode):
    toolchain = _get_toolchain(ctx)
    if toolchain.target_os == "darwin" or toolchain.target_os == "windows":
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
    "_toolchain": attr.label(default = Label("//rust/toolchain:current_rust_toolchain")),
})
bin_has_native_dep_and_alwayslink_test = analysistest.make(_bin_has_native_dep_and_alwayslink_test_impl, attrs = {
    "_toolchain": attr.label(default = Label("//rust/toolchain:current_rust_toolchain")),
})
cdylib_has_native_dep_and_alwayslink_test = analysistest.make(_cdylib_has_native_dep_and_alwayslink_test_impl, attrs = {
    "_toolchain": attr.label(default = Label("//rust/toolchain:current_rust_toolchain")),
})

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
    bin_has_native_dep_and_alwayslink_test(
        name = "bin_has_native_dep_and_alwayslink_test",
        target_under_test = ":bin_has_native_dep_and_alwayslink",
    )
    cdylib_has_native_dep_and_alwayslink_test(
        name = "cdylib_has_native_dep_and_alwayslink_test",
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
            ":bin_has_native_dep_and_alwayslink_test",
            ":bin_has_native_libs_test",
            ":cdylib_has_additional_deps_test",
            ":cdylib_has_native_dep_and_alwayslink_test",
            ":cdylib_has_native_libs_test",
            ":lib_has_no_additional_deps_test",
            ":native_linkopts_propagate_test",
            ":proc_macro_has_native_libs_test",
            ":rlib_has_no_native_libs_test",
            ":staticlib_has_native_libs_test",
        ],
    )
