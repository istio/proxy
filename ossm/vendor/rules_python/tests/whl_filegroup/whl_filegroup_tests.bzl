"""Test for py_wheel."""

load("@rules_testing//lib:analysis_test.bzl", "analysis_test", "test_suite")
load("@rules_testing//lib:util.bzl", "util")
load("//python:pip.bzl", "whl_filegroup")

def _test_runfiles(name):
    for runfiles in [True, False]:
        util.helper_target(
            whl_filegroup,
            name = name + "_subject_runfiles_{}".format(runfiles),
            whl = ":wheel",
            runfiles = runfiles,
        )
    analysis_test(
        name = name,
        impl = _test_runfiles_impl,
        targets = {
            "no_runfiles": name + "_subject_runfiles_False",
            "with_runfiles": name + "_subject_runfiles_True",
        },
    )

def _test_runfiles_impl(env, targets):
    env.expect.that_target(targets.with_runfiles).runfiles().contains_exactly([env.ctx.workspace_name + "/{package}/{name}"])
    env.expect.that_target(targets.no_runfiles).runfiles().contains_exactly([])

def whl_filegroup_test_suite(name):
    """Create the test suite.

    Args:
        name: the name of the test suite
    """
    test_suite(name = name, tests = [_test_runfiles])
