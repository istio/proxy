"""Unittest to verify that we can treat all dependencies as direct dependencies"""

load("@bazel_skylib//lib:unittest.bzl", "analysistest")
load("//test/unit:common.bzl", "assert_action_mnemonic", "assert_env_value")
load("//test/unit/consistent_crate_name:with_modified_crate_name.bzl", "with_modified_crate_name")

def _consistent_crate_name_env_test(ctx):
    env = analysistest.begin(ctx)
    tut = analysistest.target_under_test(env)
    action = tut.actions[0]
    assert_action_mnemonic(env, action, "Rustc")
    assert_env_value(
        env,
        action,
        "CARGO_CRATE_NAME",
        "lib_my_custom_crate_suffix",
    )
    return analysistest.end(env)

consistent_crate_name_env_test = analysistest.make(_consistent_crate_name_env_test)

def _consistent_crate_name_test():
    with_modified_crate_name(
        name = "lib",
        src = "lib.rs",
    )

    consistent_crate_name_env_test(
        name = "consistent_crate_name_env_test",
        target_under_test = ":lib",
    )

def consistent_crate_name_test_suite(name):
    """Entry-point macro called from the BUILD file.

    Args:
        name: Name of the macro.
    """
    _consistent_crate_name_test()

    native.test_suite(
        name = name,
        tests = [
            ":consistent_crate_name_env_test",
        ],
    )
