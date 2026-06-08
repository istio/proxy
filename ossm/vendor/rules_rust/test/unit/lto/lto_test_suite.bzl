"""Starlark tests for `//rust/settings/lto`"""

load("@bazel_skylib//lib:unittest.bzl", "analysistest")
load("@bazel_skylib//rules:write_file.bzl", "write_file")
load("//rust:defs.bzl", "rust_library", "rust_proc_macro")
load(
    "//test/unit:common.bzl",
    "assert_action_mnemonic",
    "assert_argv_contains",
    "assert_argv_contains_not",
    "assert_argv_contains_prefix_not",
)

def _lto_test_impl(ctx, lto_setting, embed_bitcode, linker_plugin):
    env = analysistest.begin(ctx)
    target = analysistest.target_under_test(env)

    action = target.actions[0]
    assert_action_mnemonic(env, action, "Rustc")

    # Check if LTO is enabled.
    if lto_setting:
        assert_argv_contains(env, action, "-Clto={}".format(lto_setting))
    else:
        assert_argv_contains_prefix_not(env, action, "-Clto")

    # Check if we should embed bitcode.
    if embed_bitcode:
        assert_argv_contains(env, action, "-Cembed-bitcode={}".format(embed_bitcode))
    else:
        assert_argv_contains_prefix_not(env, action, "-Cembed-bitcode")

    # Check if we should use linker plugin LTO.
    if linker_plugin:
        assert_argv_contains(env, action, "-Clinker-plugin-lto")
    else:
        assert_argv_contains_not(env, action, "-Clinker-plugin-lto")

    return analysistest.end(env)

def _lto_level_default(ctx):
    return _lto_test_impl(ctx, None, "no", False)

_lto_level_default_test = analysistest.make(
    _lto_level_default,
    config_settings = {},
)

def _lto_level_manual(ctx):
    return _lto_test_impl(ctx, None, None, False)

_lto_level_manual_test = analysistest.make(
    _lto_level_manual,
    config_settings = {str(Label("//rust/settings:lto")): "manual"},
)

def _lto_level_off(ctx):
    return _lto_test_impl(ctx, "off", "no", False)

_lto_level_off_test = analysistest.make(
    _lto_level_off,
    config_settings = {str(Label("//rust/settings:lto")): "off"},
)

def _lto_level_thin(ctx):
    return _lto_test_impl(ctx, "thin", None, True)

_lto_level_thin_test = analysistest.make(
    _lto_level_thin,
    config_settings = {str(Label("//rust/settings:lto")): "thin"},
)

def _lto_level_fat(ctx):
    return _lto_test_impl(ctx, "fat", None, True)

_lto_level_fat_test = analysistest.make(
    _lto_level_fat,
    config_settings = {str(Label("//rust/settings:lto")): "fat"},
)

def _lto_proc_macro(ctx):
    return _lto_test_impl(ctx, None, "no", False)

_lto_proc_macro_test = analysistest.make(
    _lto_proc_macro,
    config_settings = {str(Label("//rust/settings:lto")): "thin"},
)

def lto_test_suite(name):
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

    rust_proc_macro(
        name = "proc_macro",
        srcs = [":lib.rs"],
        edition = "2021",
    )

    _lto_level_default_test(
        name = "lto_level_default_test",
        target_under_test = ":lib",
    )

    _lto_level_manual_test(
        name = "lto_level_manual_test",
        target_under_test = ":lib",
    )

    _lto_level_off_test(
        name = "lto_level_off_test",
        target_under_test = ":lib",
    )

    _lto_level_thin_test(
        name = "lto_level_thin_test",
        target_under_test = ":lib",
    )

    _lto_level_fat_test(
        name = "lto_level_fat_test",
        target_under_test = ":lib",
    )

    _lto_proc_macro_test(
        name = "lto_proc_macro_test",
        target_under_test = ":proc_macro",
    )

    native.test_suite(
        name = name,
        tests = [
            ":lto_level_default_test",
            ":lto_level_manual_test",
            ":lto_level_off_test",
            ":lto_level_thin_test",
            ":lto_level_fat_test",
            ":lto_proc_macro_test",
        ],
    )
