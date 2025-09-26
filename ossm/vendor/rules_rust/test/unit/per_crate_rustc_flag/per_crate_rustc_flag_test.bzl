"""Unittest to verify per_crate_rustc_flag (attribute) propagation"""

load("@bazel_skylib//lib:unittest.bzl", "analysistest", "asserts")
load("//rust:defs.bzl", "rust_library")
load("//test/unit:common.bzl", "assert_argv_contains", "assert_argv_contains_not")

RUSTC_FLAG = "--codegen=linker-plugin-lto"
MATCHING_LABEL_FLAG = "//test/unit/per_crate_rustc_flag:lib@--codegen=linker-plugin-lto"
MATCHING_EXECUTION_PATH_FLAG = "test/unit/per_crate_rustc_flag/lib.rs@--codegen=linker-plugin-lto"
NON_MATCHING_FLAG = "rust/private/rustc.bzl@--codegen=linker-plugin-lto"
INVALID_FLAG = "--codegen=linker-plugin-lto"

def target_action_contains_not_flag(env, target):
    action = target.actions[0]
    asserts.equals(env, "Rustc", action.mnemonic)

    assert_argv_contains_not(
        env = env,
        action = action,
        flag = RUSTC_FLAG,
    )

def target_action_contains_flag(env, target):
    action = target.actions[0]
    asserts.equals(env, "Rustc", action.mnemonic)

    assert_argv_contains(
        env = env,
        action = action,
        flag = RUSTC_FLAG,
    )

def _per_crate_rustc_flag_not_present_test(ctx):
    env = analysistest.begin(ctx)
    target = analysistest.target_under_test(env)
    target_action_contains_not_flag(env, target)

    return analysistest.end(env)

def _per_crate_rustc_flag_present_test(ctx):
    env = analysistest.begin(ctx)
    target = analysistest.target_under_test(env)
    target_action_contains_flag(env, target)

    # Check the exec configuration target does NOT contain.
    target = ctx.attr.lib_exec
    target_action_contains_not_flag(env, target)

    return analysistest.end(env)

def _per_crate_rustc_flag_invalid_pattern_test(ctx):
    env = analysistest.begin(ctx)
    asserts.expect_failure(env, "does not follow the expected format: prefix_filter@flag")

    return analysistest.end(env)

per_crate_rustc_flag_not_present_test = analysistest.make(_per_crate_rustc_flag_not_present_test)

per_crate_rustc_flag_not_present_non_matching_test = analysistest.make(
    _per_crate_rustc_flag_not_present_test,
    config_settings = {
        str(Label("//rust/settings:experimental_per_crate_rustc_flag")): [NON_MATCHING_FLAG],
    },
)

per_crate_rustc_flag_present_label_test = analysistest.make(
    _per_crate_rustc_flag_present_test,
    attrs = {
        "lib_exec": attr.label(
            mandatory = True,
            cfg = "exec",
        ),
    },
    config_settings = {
        str(Label("//rust/settings:experimental_per_crate_rustc_flag")): [MATCHING_LABEL_FLAG],
    },
)

per_crate_rustc_flag_present_execution_path_test = analysistest.make(
    _per_crate_rustc_flag_present_test,
    attrs = {
        "lib_exec": attr.label(
            mandatory = True,
            cfg = "exec",
        ),
    },
    config_settings = {
        str(Label("//rust/settings:experimental_per_crate_rustc_flag")): [MATCHING_EXECUTION_PATH_FLAG],
    },
)

per_crate_rustc_flag_invalid_pattern_test = analysistest.make(
    _per_crate_rustc_flag_invalid_pattern_test,
    expect_failure = True,
    config_settings = {
        str(Label("//rust/settings:experimental_per_crate_rustc_flag")): [INVALID_FLAG],
    },
)

def _define_test_targets():
    rust_library(
        name = "lib",
        srcs = ["lib.rs"],
        edition = "2018",
    )

def per_crate_rustc_flag_test_suite(name):
    """Entry-point macro called from the BUILD file.

    Args:
        name (str): Name of the macro.
    """

    _define_test_targets()

    per_crate_rustc_flag_not_present_test(
        name = "per_crate_rustc_flag_not_present_test",
        target_under_test = ":lib",
    )

    per_crate_rustc_flag_not_present_non_matching_test(
        name = "per_crate_rustc_flag_not_present_non_matching_test",
        target_under_test = ":lib",
    )

    per_crate_rustc_flag_present_label_test(
        name = "per_crate_rustc_flag_present_label_test",
        target_under_test = ":lib",
        lib_exec = ":lib",
    )

    per_crate_rustc_flag_present_execution_path_test(
        name = "per_crate_rustc_flag_present_execution_path_test",
        target_under_test = ":lib",
        lib_exec = ":lib",
    )

    per_crate_rustc_flag_invalid_pattern_test(
        name = "per_crate_rustc_flag_invalid_pattern_test",
        target_under_test = ":lib",
    )

    native.test_suite(
        name = name,
        tests = [
            ":per_crate_rustc_flag_not_present_test",
            ":per_crate_rustc_flag_not_present_non_matching_test",
            ":per_crate_rustc_flag_present_label_test",
            ":per_crate_rustc_flag_present_execution_path_test",
            ":per_crate_rustc_flag_invalid_pattern_test",
        ],
    )
