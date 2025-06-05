"""Unittest to verify re-exported symbols propagate to downstream crates"""

load("@bazel_skylib//lib:unittest.bzl", "analysistest", "asserts")
load("//test/unit:common.bzl", "assert_argv_contains_prefix", "assert_argv_contains_prefix_suffix")

def _exports_test_impl(ctx, dependencies, externs):
    env = analysistest.begin(ctx)
    target = analysistest.target_under_test(env)

    action = target.actions[0]
    asserts.equals(env, action.mnemonic, "Rustc")

    # Transitive symbols that get re-exported are expected to be located by a `-Ldependency` flag.
    # The assert below ensures that each dependnecy flag is passed to the Rustc action. For details see
    # https://doc.rust-lang.org/rustc/command-line-arguments.html#-l-add-a-directory-to-the-library-search-path
    for dep in dependencies:
        assert_argv_contains_prefix_suffix(
            env = env,
            action = action,
            prefix = "-Ldependency",
            suffix = dep,
        )

    for dep in externs:
        assert_argv_contains_prefix(
            env = env,
            action = action,
            prefix = "--extern={}=".format(dep),
        )

    return analysistest.end(env)

def _lib_exports_test_impl(ctx):
    # This test is only expected to be used with
    # `//test/unit/exports/lib_c`
    return _exports_test_impl(
        ctx = ctx,
        dependencies = ["lib_a", "lib_b"],
        externs = ["lib_b"],
    )

def _test_exports_test_impl(ctx):
    # This test is only expected to be used with
    # `//test/unit/exports/lib_c:lib_c_test`
    return _exports_test_impl(
        ctx = ctx,
        dependencies = ["lib_a", "lib_b"],
        externs = ["lib_b"],
    )

lib_exports_test = analysistest.make(_lib_exports_test_impl)
test_exports_test = analysistest.make(_test_exports_test_impl)

def exports_test_suite(name):
    """Entry-point macro called from the BUILD file.

    Args:
        name (str): Name of the macro.
    """

    lib_exports_test(
        name = "lib_exports_test",
        target_under_test = "//test/unit/exports/lib_c",
    )

    test_exports_test(
        name = "test_exports_test",
        target_under_test = "//test/unit/exports/lib_c:lib_c_test",
    )

    native.test_suite(
        name = name,
        tests = [
            ":lib_exports_test",
            ":test_exports_test",
        ],
    )
