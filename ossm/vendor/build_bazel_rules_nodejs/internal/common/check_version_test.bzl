"Unit tests for check_version.bzl"

load("@bazel_skylib//lib:unittest.bzl", "asserts", "unittest")
load(":check_version.bzl", "check_version", "check_version_range")

def _check_version_test_impl(ctx):
    env = unittest.begin(ctx)
    asserts.equals(env, False, check_version("1.2.2", "1.2.3"))
    asserts.equals(env, True, check_version("1.12.3", "1.2.1"))
    asserts.equals(env, True, check_version("0.8.0rc2", "0.8.0"))
    asserts.equals(env, True, check_version("0.8.0+custombuild", "0.8.0"))
    asserts.equals(env, True, check_version_range("1.2.2", "1.2.1", "1.2.3"))
    asserts.equals(env, False, check_version_range("1.2.0", "1.2.1", "1.2.3"))
    asserts.equals(env, False, check_version_range("1.2.4", "1.2.1", "1.2.3"))

    return unittest.end(env)

check_version_test = unittest.make(_check_version_test_impl)

def check_version_test_suite():
    unittest.suite("check_version_tests", check_version_test)
