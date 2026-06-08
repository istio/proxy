"""Tests for toolchain channel transitions"""

load("@bazel_skylib//lib:unittest.bzl", "analysistest")
load("@bazel_skylib//rules:write_file.bzl", "write_file")
load("//rust:defs.bzl", "rust_binary")
load(
    "//test/unit:common.bzl",
    "assert_action_mnemonic",
    "assert_argv_contains",
    "assert_argv_contains_not",
)

def _channel_transition_test_impl(ctx, is_nightly):
    env = analysistest.begin(ctx)
    target = analysistest.target_under_test(env)

    action = target.actions[0]
    assert_action_mnemonic(env, action, "Rustc")

    if is_nightly:
        assert_argv_contains(env, action, "-Zunstable-options")
    else:
        assert_argv_contains_not(env, action, "-Zunstable-options")

    return analysistest.end(env)

def _nightly_transition_test_impl(ctx):
    return _channel_transition_test_impl(ctx, True)

nightly_transition_test = analysistest.make(
    _nightly_transition_test_impl,
    doc = "Test that targets can be forced to use a nightly toolchain",
    config_settings = {
        str(Label("//rust/toolchain/channel:channel")): "nightly",
    },
)

def _stable_transition_test_impl(ctx):
    return _channel_transition_test_impl(ctx, False)

stable_transition_test = analysistest.make(
    _stable_transition_test_impl,
    doc = "Test that targets can be forced to use a stable toolchain",
    config_settings = {
        str(Label("//rust/toolchain/channel:channel")): "stable",
    },
)

def channel_transitions_test_suite(name):
    """Entry-point macro called from the BUILD file.

    Args:
        name (str): Name of the macro.
    """

    write_file(
        name = "main_rs",
        out = "main.rs",
        content = [
            "fn main() {}",
            "",
        ],
    )

    rust_binary(
        name = "bin",
        srcs = ["main.rs"],
        edition = "2018",
        rustc_flags = select({
            "@rules_rust//rust/toolchain/channel:nightly": ["-Zunstable-options"],
            "//conditions:default": [],
        }),
    )

    nightly_transition_test(
        name = "nightly_transition_test",
        target_under_test = ":bin",
    )

    stable_transition_test(
        name = "stable_transition_test",
        target_under_test = ":bin",
    )

    native.test_suite(
        name = name,
        tests = [
            ":nightly_transition_test",
            ":stable_transition_test",
        ],
    )
