"""Unittest to verify location expansion in rustc flags"""

load("@bazel_skylib//lib:unittest.bzl", "analysistest")
load("@bazel_skylib//rules:write_file.bzl", "write_file")
load("//rust:defs.bzl", "rust_library")
load("//test/unit:common.bzl", "assert_action_mnemonic", "assert_argv_contains")

def _location_expansion_rustc_flags_test(ctx):
    env = analysistest.begin(ctx)
    tut = analysistest.target_under_test(env)
    action = tut.actions[0]
    assert_action_mnemonic(env, action, "Rustc")
    assert_argv_contains(env, action, "test/unit/location_expansion/mylibrary.rs")
    expected = "@${pwd}/" + ctx.bin_dir.path + "/test/unit/location_expansion/generated_flag.data"
    assert_argv_contains(env, action, expected)
    return analysistest.end(env)

location_expansion_rustc_flags_test = analysistest.make(_location_expansion_rustc_flags_test)

def _location_expansion_test():
    write_file(
        name = "flag_generator",
        out = "generated_flag.data",
        content = [
            "--cfg=test_flag",
            "",
        ],
        newline = "unix",
    )

    rust_library(
        name = "mylibrary",
        srcs = ["mylibrary.rs"],
        edition = "2018",
        rustc_flags = [
            "@$(location :flag_generator)",
        ],
        compile_data = [":flag_generator"],
    )

    location_expansion_rustc_flags_test(
        name = "location_expansion_rustc_flags_test",
        target_under_test = ":mylibrary",
    )

def location_expansion_test_suite(name):
    """Entry-point macro called from the BUILD file.

    Args:
        name: Name of the macro.
    """
    _location_expansion_test()

    native.test_suite(
        name = name,
        tests = [
            ":location_expansion_rustc_flags_test",
        ],
    )
