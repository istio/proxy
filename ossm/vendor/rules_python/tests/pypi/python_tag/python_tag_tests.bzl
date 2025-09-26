""

load("@rules_testing//lib:test_suite.bzl", "test_suite")
load("//python/private/pypi:python_tag.bzl", "python_tag")  # buildifier: disable=bzl-visibility

_tests = []

def _test_without_version(env):
    for give, expect in {
        "cpython": "cp",
        "ironpython": "ip",
        "jython": "jy",
        "pypy": "pp",
        "python": "py",
        "something_else": "something_else",
    }.items():
        got = python_tag(give)
        env.expect.that_str(got).equals(expect)

_tests.append(_test_without_version)

def _test_with_version(env):
    got = python_tag("cpython", "3.1.15")
    env.expect.that_str(got).equals("cp31")

_tests.append(_test_with_version)

def python_tag_test_suite(name):
    """Create the test suite.

    Args:
        name: the name of the test suite
    """
    test_suite(name = name, basic_tests = _tests)
