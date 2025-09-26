"""Unittest to verify compile_data (attribute) propagation"""

load("@bazel_skylib//lib:unittest.bzl", "analysistest", "asserts")
load("//rust:defs.bzl", "rust_library")
load("//test/unit:common.bzl", "assert_argv_contains", "assert_argv_contains_not")

EXTRA_FLAG = "--codegen=linker-plugin-lto"

def target_action_contains_not_flag(env, target):
    action = target.actions[0]
    asserts.equals(env, "Rustc", action.mnemonic)

    assert_argv_contains_not(
        env = env,
        action = action,
        flag = EXTRA_FLAG,
    )

def target_action_contains_flag(env, target):
    action = target.actions[0]
    asserts.equals(env, "Rustc", action.mnemonic)

    assert_argv_contains(
        env = env,
        action = action,
        flag = EXTRA_FLAG,
    )

def _extra_rustc_flags_not_present_test(ctx):
    env = analysistest.begin(ctx)
    target = analysistest.target_under_test(env)
    target_action_contains_not_flag(env, target)

    return analysistest.end(env)

def _extra_rustc_flags_present_test(ctx):
    env = analysistest.begin(ctx)
    target = analysistest.target_under_test(env)
    target_action_contains_flag(env, target)

    # Check the exec configuration target does NOT contain.
    target = ctx.attr.lib_exec
    target_action_contains_not_flag(env, target)

    return analysistest.end(env)

extra_rustc_flags_not_present_test = analysistest.make(_extra_rustc_flags_not_present_test)
extra_rustc_flags_present_test = analysistest.make(
    _extra_rustc_flags_present_test,
    attrs = {
        "lib_exec": attr.label(
            mandatory = True,
            cfg = "exec",
        ),
    },
    config_settings = {
        str(Label("//rust/settings:extra_rustc_flags")): [EXTRA_FLAG],
    },
)

extra_rustc_flag_present_test = analysistest.make(
    _extra_rustc_flags_present_test,
    attrs = {
        "lib_exec": attr.label(
            mandatory = True,
            cfg = "exec",
        ),
    },
    config_settings = {
        str(Label("//rust/settings:extra_rustc_flag")): [EXTRA_FLAG],
        str(Label("//rust/settings:extra_rustc_flags")): [],
    },
)

def _define_test_targets():
    rust_library(
        name = "lib",
        srcs = ["lib.rs"],
        edition = "2018",
    )

def extra_rustc_flags_test_suite(name):
    """Entry-point macro called from the BUILD file.

    Args:
        name (str): Name of the macro.
    """

    _define_test_targets()

    extra_rustc_flags_not_present_test(
        name = "extra_rustc_flags_not_present_test",
        target_under_test = ":lib",
    )

    extra_rustc_flags_present_test(
        name = "extra_rustc_flags_present_test",
        target_under_test = ":lib",
        lib_exec = ":lib",
    )

    extra_rustc_flag_present_test(
        name = "extra_rustc_flag_present_test",
        target_under_test = ":lib",
        lib_exec = ":lib",
    )

    native.test_suite(
        name = name,
        tests = [
            ":extra_rustc_flags_not_present_test",
            ":extra_rustc_flags_present_test",
        ],
    )
