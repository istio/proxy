"""Starlark tests for `//rust/settings:codegen_units`"""

load("@bazel_skylib//lib:unittest.bzl", "analysistest")
load("@bazel_skylib//rules:write_file.bzl", "write_file")
load("//rust:defs.bzl", "rust_binary", "rust_library")
load(
    "//test/unit:common.bzl",
    "assert_action_mnemonic",
    "assert_argv_contains",
    "assert_argv_contains_prefix_not",
)

_EXPECTED_VALUE = 11

def _codegen_units_test_impl(ctx, enabled = True):
    env = analysistest.begin(ctx)
    target = analysistest.target_under_test(env)

    action = target.actions[0]
    assert_action_mnemonic(env, action, "Rustc")

    if enabled:
        assert_argv_contains(env, action, "-Ccodegen-units={}".format(_EXPECTED_VALUE))
    else:
        assert_argv_contains_prefix_not(env, action, "-Ccodegen-units")

    return analysistest.end(env)

_codegen_units_test = analysistest.make(
    _codegen_units_test_impl,
    config_settings = {str(Label("//rust/settings:codegen_units")): _EXPECTED_VALUE},
)

def _codegen_units_negative_value_test_impl(ctx):
    return _codegen_units_test_impl(ctx, enabled = False)

_codegen_units_negative_value_test = analysistest.make(
    _codegen_units_negative_value_test_impl,
    config_settings = {str(Label("//rust/settings:codegen_units")): -1},
)

def codegen_units_test_suite(name):
    """Entry-point macro called from the BUILD file.

    Args:
        name (str): The name of the test suite.
    """
    write_file(
        name = "crate_lib",
        out = "lib.rs",
        content = [
            "#[allow(dead_code)]",
            "fn add() {}",
            "",
        ],
    )

    rust_library(
        name = "lib",
        srcs = [":lib.rs"],
        edition = "2021",
    )

    write_file(
        name = "crate_bin",
        out = "bin.rs",
        content = [
            "fn main() {}",
            "",
        ],
    )

    rust_binary(
        name = "bin",
        srcs = [":bin.rs"],
        edition = "2021",
    )

    _codegen_units_test(
        name = "codegen_units_lib_test",
        target_under_test = ":lib",
    )

    _codegen_units_test(
        name = "codegen_units_bin_test",
        target_under_test = ":bin",
    )

    _codegen_units_negative_value_test(
        name = "codegen_units_negative_value_lib_test",
        target_under_test = ":lib",
    )

    _codegen_units_negative_value_test(
        name = "codegen_units_negative_value_bin_test",
        target_under_test = ":bin",
    )

    native.test_suite(
        name = name,
        tests = [
            ":codegen_units_lib_test",
            ":codegen_units_bin_test",
            ":codegen_units_negative_value_lib_test",
            ":codegen_units_negative_value_bin_test",
        ],
    )
