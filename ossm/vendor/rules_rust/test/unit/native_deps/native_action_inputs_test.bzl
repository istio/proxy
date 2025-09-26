"""Unittests for rust rules."""

load("@bazel_skylib//lib:unittest.bzl", "analysistest", "asserts")
load(
    "//rust:defs.bzl",
    "rust_binary",
    "rust_common",
    "rust_library",
    "rust_proc_macro",
    "rust_shared_library",
    "rust_static_library",
)
load("//test/unit:common.bzl", "assert_action_mnemonic")

def _get_crate_info(target):
    return target[rust_common.crate_info] if rust_common.crate_info in target else target[rust_common.test_crate_info].crate

def _native_action_inputs_present_test_impl(ctx):
    env = analysistest.begin(ctx)
    tut = analysistest.target_under_test(env)
    action = tut.actions[0]
    assert_action_mnemonic(env, action, "Rustc")
    inputs = action.inputs.to_list()
    for_shared_library = _get_crate_info(tut).type in ("dylib", "cdylib", "proc-macro")
    lib_name = _native_dep_lib_name(ctx, for_shared_library)

    asserts.true(
        env,
        _has_action_input(lib_name, inputs),
        "Expected to contain {lib_name} as action input, got {inputs}".format(
            lib_name = lib_name,
            inputs = inputs,
        ),
    )

    return analysistest.end(env)

def _native_action_inputs_not_present_test_impl(ctx):
    env = analysistest.begin(ctx)
    tut = analysistest.target_under_test(env)
    action = tut.actions[0]
    for_shared_library = _get_crate_info(tut).type in ("dylib", "cdylib", "proc-macro")
    assert_action_mnemonic(env, action, "Rustc")
    inputs = action.inputs.to_list()
    lib_name = _native_dep_lib_name(ctx, for_shared_library)

    asserts.false(
        env,
        _has_action_input(lib_name, inputs),
        "Expected not to contain {lib_name}".format(lib_name = lib_name),
    )

    return analysistest.end(env)

def _native_dep_lib_name(ctx, for_shared_library):
    compilation_mode = ctx.var["COMPILATION_MODE"]
    if ctx.target_platform_has_constraint(
        ctx.attr._windows_constraint[platform_common.ConstraintValueInfo],
    ):
        return "bar.lib"
    if ctx.target_platform_has_constraint(
        ctx.attr._macos_constraint[platform_common.ConstraintValueInfo],
    ):
        pic_suffix = ""
    else:
        pic_suffix = ".pic" if compilation_mode == "opt" and for_shared_library else ""
    return "libbar{}.a".format(pic_suffix)

def _has_action_input(name, inputs):
    for file in inputs:
        if file.basename == name:
            return True
    return False

native_action_inputs_present_test = analysistest.make(
    _native_action_inputs_present_test_impl,
    attrs = {
        "_macos_constraint": attr.label(default = Label("@platforms//os:macos")),
        "_windows_constraint": attr.label(default = Label("@platforms//os:windows")),
    },
)
native_action_inputs_not_present_test = analysistest.make(
    _native_action_inputs_not_present_test_impl,
    attrs = {
        "_macos_constraint": attr.label(default = Label("@platforms//os:macos")),
        "_windows_constraint": attr.label(default = Label("@platforms//os:windows")),
    },
)

def _native_action_inputs_test():
    rust_library(
        name = "foo_lib",
        srcs = ["foo.rs"],
        edition = "2018",
        deps = [":bar"],
    )

    rust_binary(
        name = "foo_bin",
        srcs = ["foo_main.rs"],
        edition = "2018",
        deps = [":bar"],
    )

    rust_shared_library(
        name = "foo_dylib",
        srcs = ["foo.rs"],
        edition = "2018",
        deps = [":bar"],
    )

    rust_static_library(
        name = "foo_static",
        srcs = ["foo.rs"],
        edition = "2018",
        deps = [":bar"],
    )

    rust_proc_macro(
        name = "foo_proc_macro",
        srcs = ["foo.rs"],
        edition = "2018",
        deps = [":bar"],
    )

    # buildifier: disable=native-cc
    native.cc_library(
        name = "bar",
        srcs = ["bar.cc"],
    )

    native_action_inputs_not_present_test(
        name = "native_action_inputs_lib_test",
        target_under_test = ":foo_lib",
    )

    native_action_inputs_present_test(
        name = "native_action_inputs_bin_test",
        target_under_test = ":foo_bin",
    )

    native_action_inputs_present_test(
        name = "native_action_inputs_dylib_test",
        target_under_test = ":foo_dylib",
    )

    native_action_inputs_present_test(
        name = "native_action_inputs_static_test",
        target_under_test = ":foo_static",
    )

    native_action_inputs_present_test(
        name = "native_action_inputs_proc_macro_test",
        target_under_test = ":foo_proc_macro",
    )

def native_action_inputs_test_suite(name):
    """Entry-point macro called from the BUILD file.

    Args:
        name: Name of the macro.
    """
    _native_action_inputs_test()

    native.test_suite(
        name = name,
        tests = [
            ":native_action_inputs_lib_test",
            ":native_action_inputs_bin_test",
            ":native_action_inputs_dylib_test",
            ":native_action_inputs_static_test",
            ":native_action_inputs_proc_macro_test",
        ],
    )
