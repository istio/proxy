"""Unit tests for semver.bzl."""

load("@bazel_skylib//lib:unittest.bzl", "asserts", "unittest")

# buildifier: disable=bzl-visibility
load("//rust/private:semver.bzl", "semver")

def _semver_basic_test_impl(ctx):
    env = unittest.begin(ctx)

    # Test basic semver parsing
    result = semver("1.2.3")
    asserts.equals(env, 1, result.major)
    asserts.equals(env, 2, result.minor)
    asserts.equals(env, 3, result.patch)
    asserts.equals(env, None, result.pre)
    asserts.equals(env, "1.2.3", result.str)

    # Test with zeros
    result = semver("0.0.0")
    asserts.equals(env, 0, result.major)
    asserts.equals(env, 0, result.minor)
    asserts.equals(env, 0, result.patch)
    asserts.equals(env, None, result.pre)
    asserts.equals(env, "0.0.0", result.str)

    # Test larger version numbers
    result = semver("10.20.30")
    asserts.equals(env, 10, result.major)
    asserts.equals(env, 20, result.minor)
    asserts.equals(env, 30, result.patch)
    asserts.equals(env, None, result.pre)
    asserts.equals(env, "10.20.30", result.str)

    return unittest.end(env)

def _semver_with_pre_test_impl(ctx):
    env = unittest.begin(ctx)

    # Test semver with pre-release
    result = semver("1.2.3-rc4")
    asserts.equals(env, 1, result.major)
    asserts.equals(env, 2, result.minor)
    asserts.equals(env, 3, result.patch)
    asserts.equals(env, "rc4", result.pre)
    asserts.equals(env, "1.2.3-rc4", result.str)

    # Test semver with alpha pre-release
    result = semver("2.0.0-alpha")
    asserts.equals(env, 2, result.major)
    asserts.equals(env, 0, result.minor)
    asserts.equals(env, 0, result.patch)
    asserts.equals(env, "alpha", result.pre)
    asserts.equals(env, "2.0.0-alpha", result.str)

    # Test semver with beta pre-release
    result = semver("1.5.0-beta.1")
    asserts.equals(env, 1, result.major)
    asserts.equals(env, 5, result.minor)
    asserts.equals(env, 0, result.patch)
    asserts.equals(env, "beta.1", result.pre)
    asserts.equals(env, "1.5.0-beta.1", result.str)

    # Test semver with nightly pre-release
    result = semver("1.70.0-nightly")
    asserts.equals(env, 1, result.major)
    asserts.equals(env, 70, result.minor)
    asserts.equals(env, 0, result.patch)
    asserts.equals(env, "nightly", result.pre)
    asserts.equals(env, "1.70.0-nightly", result.str)

    return unittest.end(env)

def _semver_edge_cases_test_impl(ctx):
    env = unittest.begin(ctx)

    # Test semver with empty pre-release (trailing dash)
    # When there's a trailing dash, partition returns empty string for pre,
    # but "pre or None" converts it to None
    result = semver("1.2.3-")
    asserts.equals(env, 1, result.major)
    asserts.equals(env, 2, result.minor)
    asserts.equals(env, 3, result.patch)
    asserts.equals(env, "", result.pre)
    asserts.equals(env, "1.2.3-", result.str)

    # Test semver with multiple dashes in pre-release
    result = semver("1.2.3-alpha-test")
    asserts.equals(env, 1, result.major)
    asserts.equals(env, 2, result.minor)
    asserts.equals(env, 3, result.patch)
    asserts.equals(env, "alpha-test", result.pre)
    asserts.equals(env, "1.2.3-alpha-test", result.str)

    return unittest.end(env)

def _semver_real_world_examples_test_impl(ctx):
    env = unittest.begin(ctx)

    # Test real Rust version examples
    result = semver("1.80.0")
    asserts.equals(env, 1, result.major)
    asserts.equals(env, 80, result.minor)
    asserts.equals(env, 0, result.patch)
    asserts.equals(env, None, result.pre)
    asserts.equals(env, "1.80.0", result.str)

    result = semver("1.70.0")
    asserts.equals(env, 1, result.major)
    asserts.equals(env, 70, result.minor)
    asserts.equals(env, 0, result.patch)
    asserts.equals(env, None, result.pre)

    result = semver("1.54.0")
    asserts.equals(env, 1, result.major)
    asserts.equals(env, 54, result.minor)
    asserts.equals(env, 0, result.patch)
    asserts.equals(env, None, result.pre)

    return unittest.end(env)

semver_basic_test = unittest.make(_semver_basic_test_impl)
semver_with_pre_test = unittest.make(_semver_with_pre_test_impl)
semver_edge_cases_test = unittest.make(_semver_edge_cases_test_impl)
semver_real_world_examples_test = unittest.make(_semver_real_world_examples_test_impl)

def semver_test_suite(name):
    """Entry-point macro called from the BUILD file.

    Args:
        name (str): Name of the test suite.
    """
    unittest.suite(
        name,
        semver_basic_test,
        semver_with_pre_test,
        semver_edge_cases_test,
        semver_real_world_examples_test,
    )
