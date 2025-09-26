"""Unittest to verify ordering of rust stdlib in rust_library() CcInfo"""

load("@bazel_skylib//lib:unittest.bzl", "analysistest")
load("//rust:defs.bzl", "rust_test")
load("//test/unit:common.bzl", "assert_action_mnemonic", "assert_argv_contains", "assert_argv_contains_not", "assert_list_contains_adjacent_elements", "assert_list_contains_adjacent_elements_not")

def _use_libtest_harness_rustc_flags_test_impl(ctx):
    env = analysistest.begin(ctx)
    tut = analysistest.target_under_test(env)
    action = tut.actions[0]
    assert_action_mnemonic(env, action, "Rustc")
    assert_argv_contains(env, action, "test/unit/use_libtest_harness/mytest.rs")
    assert_argv_contains(env, action, "--test")
    assert_list_contains_adjacent_elements_not(env, action.argv, ["--cfg", "test"])
    return analysistest.end(env)

def _use_libtest_harness_rustc_noharness_flags_test_impl(ctx):
    env = analysistest.begin(ctx)
    tut = analysistest.target_under_test(env)
    action = tut.actions[0]
    assert_action_mnemonic(env, action, "Rustc")
    assert_argv_contains(env, action, "test/unit/use_libtest_harness/mytest_noharness.rs")
    assert_argv_contains_not(env, action, "--test")
    assert_list_contains_adjacent_elements(env, action.argv, ["--cfg", "test"])
    return analysistest.end(env)

def _use_libtest_harness_rustc_noharness_main_flags_test_impl(ctx):
    env = analysistest.begin(ctx)
    tut = analysistest.target_under_test(env)
    action = tut.actions[0]
    assert_action_mnemonic(env, action, "Rustc")
    assert_argv_contains(env, action, "test/unit/use_libtest_harness/main.rs")
    assert_argv_contains_not(env, action, "--test")
    assert_list_contains_adjacent_elements(env, action.argv, ["--cfg", "test"])
    return analysistest.end(env)

use_libtest_harness_rustc_flags_test = analysistest.make(_use_libtest_harness_rustc_flags_test_impl)
use_libtest_harness_rustc_noharness_flags_test = analysistest.make(_use_libtest_harness_rustc_noharness_flags_test_impl)
use_libtest_harness_rustc_noharness_main_flags_test = analysistest.make(_use_libtest_harness_rustc_noharness_main_flags_test_impl)

def _use_libtest_harness_test():
    rust_test(
        name = "mytest",
        srcs = ["mytest.rs"],
        edition = "2018",
    )

    rust_test(
        name = "mytest_noharness",
        srcs = ["mytest_noharness.rs"],
        edition = "2018",
        use_libtest_harness = False,
    )

    rust_test(
        name = "mytest_noharness_main",
        srcs = [
            "main.rs",
            "mytest.rs",
        ],
        edition = "2018",
        use_libtest_harness = False,
    )

    use_libtest_harness_rustc_flags_test(
        name = "use_libtest_harness_rustc_flags_test",
        target_under_test = ":mytest",
    )

    use_libtest_harness_rustc_noharness_flags_test(
        name = "use_libtest_harness_rustc_noharness_flags_test",
        target_under_test = ":mytest_noharness",
    )

    use_libtest_harness_rustc_noharness_main_flags_test(
        name = "use_libtest_harness_rustc_noharness_main_flags_test",
        target_under_test = ":mytest_noharness_main",
    )

def use_libtest_harness_test_suite(name):
    """Entry-point macro called from the BUILD file.

    Args:
        name: Name of the macro.
    """
    _use_libtest_harness_test()

    native.test_suite(
        name = name,
        tests = [
            ":use_libtest_harness_rustc_flags_test",
        ],
    )
