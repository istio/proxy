"""Unittest to verify that we can treat all dependencies as direct dependencies"""

load("@bazel_skylib//lib:unittest.bzl", "analysistest")
load("//rust:defs.bzl", "rust_library")
load("//test/unit:common.bzl", "assert_action_mnemonic", "assert_argv_contains_prefix")
load("//test/unit/force_all_deps_direct:generator.bzl", "generator")

def _force_all_deps_direct_rustc_flags_test(ctx):
    env = analysistest.begin(ctx)
    tut = analysistest.target_under_test(env)
    action = tut.actions[1]
    assert_action_mnemonic(env, action, "Rustc")
    assert_argv_contains_prefix(
        env,
        action,
        "--extern=transitive",
    )
    return analysistest.end(env)

force_all_deps_direct_test = analysistest.make(_force_all_deps_direct_rustc_flags_test)

def _force_all_deps_direct_test():
    rust_library(
        name = "direct",
        srcs = ["direct.rs"],
        edition = "2018",
        deps = [":transitive"],
    )

    rust_library(
        name = "transitive",
        srcs = ["transitive.rs"],
        edition = "2018",
    )

    generator(
        name = "generate",
        deps = [":direct"],
        tags = [
            "no-clippy",
            "no-unpretty",
        ],
    )

    force_all_deps_direct_test(
        name = "force_all_deps_direct_rustc_flags_test",
        target_under_test = ":generate",
    )

def force_all_deps_direct_test_suite(name):
    """Entry-point macro called from the BUILD file.

    Args:
        name: Name of the macro.
    """
    _force_all_deps_direct_test()

    native.test_suite(
        name = name,
        tests = [
            ":force_all_deps_direct_rustc_flags_test",
        ],
    )
