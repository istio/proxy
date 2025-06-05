"""Unittests for rust rules."""

load("@bazel_skylib//lib:unittest.bzl", "analysistest", "asserts")
load(
    "//rust:defs.bzl",
    "rust_common",
    "rust_library",
    "rust_proc_macro",
    "rust_shared_library",
    "rust_static_library",
)

def _rule_provides_crate_info_test_impl(ctx):
    env = analysistest.begin(ctx)
    tut = analysistest.target_under_test(env)
    asserts.true(
        env,
        rust_common.crate_info in tut,
        "{} should provide CrateInfo".format(tut.label.name),
    )
    return analysistest.end(env)

def _rule_does_not_provide_crate_info_test_impl(ctx):
    env = analysistest.begin(ctx)
    tut = analysistest.target_under_test(env)
    asserts.false(
        env,
        rust_common.crate_info in tut,
        "{} should not provide CrateInfo".format(tut.label.name),
    )
    asserts.true(
        env,
        rust_common.test_crate_info in tut,
        "{} should provide a TestCrateInfo".format(tut.label.name),
    )
    return analysistest.end(env)

rule_provides_crate_info_test = analysistest.make(_rule_provides_crate_info_test_impl)
rule_does_not_provide_crate_info_test = analysistest.make(_rule_does_not_provide_crate_info_test_impl)

def _crate_info_test():
    rust_library(
        name = "rlib",
        srcs = ["lib.rs"],
        edition = "2018",
    )

    rust_proc_macro(
        name = "proc_macro",
        srcs = ["lib.rs"],
        edition = "2018",
    )

    rust_static_library(
        name = "staticlib",
        srcs = ["lib.rs"],
        edition = "2018",
    )

    rust_shared_library(
        name = "cdylib",
        srcs = ["lib.rs"],
        edition = "2018",
    )

    rule_provides_crate_info_test(
        name = "rlib_provides_crate_info_test",
        target_under_test = ":rlib",
    )

    rule_provides_crate_info_test(
        name = "proc_macro_provides_crate_info_test",
        target_under_test = ":proc_macro",
    )

    rule_does_not_provide_crate_info_test(
        name = "cdylib_does_not_provide_crate_info_test",
        target_under_test = ":cdylib",
    )

    rule_does_not_provide_crate_info_test(
        name = "staticlib_does_not_provide_crate_info_test",
        target_under_test = ":staticlib",
    )

def crate_info_test_suite(name):
    """Entry-point macro called from the BUILD file.

    Args:
        name: Name of the macro.
    """
    _crate_info_test()

    native.test_suite(
        name = name,
        tests = [
            ":rlib_provides_crate_info_test",
            ":proc_macro_provides_crate_info_test",
            ":cdylib_does_not_provide_crate_info_test",
            ":staticlib_does_not_provide_crate_info_test",
        ],
    )
