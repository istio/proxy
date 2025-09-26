"""Starlark tests for `rust_toolchain.opt_level`"""

load("@bazel_skylib//lib:unittest.bzl", "analysistest")
load("@bazel_skylib//rules:write_file.bzl", "write_file")
load("//rust:defs.bzl", "rust_binary")
load(
    "//test/unit:common.bzl",
    "assert_action_mnemonic",
    "assert_argv_contains",
)

def _opt_level_test_impl(ctx, expected_level):
    env = analysistest.begin(ctx)
    target = analysistest.target_under_test(env)

    action = target.actions[0]
    assert_action_mnemonic(env, action, "Rustc")

    assert_argv_contains(env, action, "--codegen=opt-level={}".format(expected_level))
    return analysistest.end(env)

def _opt_level_for_dbg_test_impl(ctx):
    return _opt_level_test_impl(ctx, "0")

_opt_level_for_dbg_test = analysistest.make(
    _opt_level_for_dbg_test_impl,
    config_settings = {
        "//command_line_option:compilation_mode": "dbg",
    },
)

def _opt_level_for_fastbuild_test_impl(ctx):
    return _opt_level_test_impl(ctx, "0")

_opt_level_for_fastbuild_test = analysistest.make(
    _opt_level_for_fastbuild_test_impl,
    config_settings = {
        "//command_line_option:compilation_mode": "fastbuild",
    },
)

def _opt_level_for_opt_test_impl(ctx):
    return _opt_level_test_impl(ctx, "3")

_opt_level_for_opt_test = analysistest.make(
    _opt_level_for_opt_test_impl,
    config_settings = {
        "//command_line_option:compilation_mode": "opt",
    },
)

def opt_level_test_suite(name):
    """Entry-point macro called from the BUILD file.

    Args:
        name (str): The name of the test suite.
    """
    write_file(
        name = "bin_main",
        out = "main.rs",
        content = [
            "fn main() {}",
            "",
        ],
    )

    rust_binary(
        name = "bin",
        srcs = [":main.rs"],
        edition = "2021",
    )

    _opt_level_for_dbg_test(
        name = "opt_level_for_dbg_test",
        target_under_test = ":bin",
    )

    _opt_level_for_fastbuild_test(
        name = "opt_level_for_fastbuild_test",
        target_under_test = ":bin",
    )

    _opt_level_for_opt_test(
        name = "opt_level_for_opt_test",
        target_under_test = ":bin",
    )

    native.test_suite(
        name = name,
        tests = [
            ":opt_level_for_dbg_test",
            ":opt_level_for_fastbuild_test",
            ":opt_level_for_opt_test",
        ],
    )
