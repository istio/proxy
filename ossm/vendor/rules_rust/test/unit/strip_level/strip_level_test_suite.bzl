"""Starlark tests for `rust_toolchain.strip_level`"""

load("@bazel_skylib//lib:unittest.bzl", "analysistest")
load("@bazel_skylib//rules:write_file.bzl", "write_file")
load("//rust:defs.bzl", "rust_binary")
load(
    "//test/unit:common.bzl",
    "assert_action_mnemonic",
    "assert_argv_contains",
)

def _strip_level_test_impl(ctx, expected_level):
    env = analysistest.begin(ctx)
    target = analysistest.target_under_test(env)

    action = target.actions[0]
    assert_action_mnemonic(env, action, "Rustc")

    assert_argv_contains(env, action, "--codegen=strip={}".format(expected_level))
    return analysistest.end(env)

def _strip_level_for_dbg_test_impl(ctx):
    return _strip_level_test_impl(ctx, "none")

_strip_level_for_dbg_test = analysistest.make(
    _strip_level_for_dbg_test_impl,
    config_settings = {
        "//command_line_option:compilation_mode": "dbg",
    },
)

def _strip_level_for_fastbuild_test_impl(ctx):
    return _strip_level_test_impl(ctx, "none")

_strip_level_for_fastbuild_test = analysistest.make(
    _strip_level_for_fastbuild_test_impl,
    config_settings = {
        "//command_line_option:compilation_mode": "fastbuild",
    },
)

def _strip_level_for_opt_test_impl(ctx):
    return _strip_level_test_impl(ctx, "debuginfo")

_strip_level_for_opt_test = analysistest.make(
    _strip_level_for_opt_test_impl,
    config_settings = {
        "//command_line_option:compilation_mode": "opt",
    },
)

def strip_level_test_suite(name):
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

    _strip_level_for_dbg_test(
        name = "strip_level_for_dbg_test",
        target_under_test = ":bin",
    )

    _strip_level_for_fastbuild_test(
        name = "strip_level_for_fastbuild_test",
        target_under_test = ":bin",
    )

    _strip_level_for_opt_test(
        name = "strip_level_for_opt_test",
        target_under_test = ":bin",
    )

    native.test_suite(
        name = name,
        tests = [
            ":strip_level_for_dbg_test",
            ":strip_level_for_fastbuild_test",
            ":strip_level_for_opt_test",
        ],
    )
