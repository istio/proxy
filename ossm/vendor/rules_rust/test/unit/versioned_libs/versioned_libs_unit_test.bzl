"""Unit tests for getting the link name of a versioned library."""

load("@bazel_skylib//lib:unittest.bzl", "asserts", "unittest")

# buildifier: disable=bzl-visibility
load("//rust/private:utils.bzl", "get_lib_name_default", "get_lib_name_for_windows")

def _produced_expected_lib_name_test_impl(ctx):
    env = unittest.begin(ctx)

    asserts.equals(env, "python", get_lib_name_default(struct(basename = "libpython.dylib")))
    asserts.equals(env, "python", get_lib_name_default(struct(basename = "libpython.so")))
    asserts.equals(env, "python", get_lib_name_default(struct(basename = "libpython.a")))
    asserts.equals(env, "python", get_lib_name_for_windows(struct(basename = "python.dll")))
    asserts.equals(env, "python", get_lib_name_for_windows(struct(basename = "python.lib")))

    asserts.equals(env, "python3", get_lib_name_default(struct(basename = "libpython3.dylib")))
    asserts.equals(env, "python3.8", get_lib_name_default(struct(basename = "libpython3.8.dylib")))
    asserts.equals(env, "python3", get_lib_name_default(struct(basename = "libpython3.a")))
    asserts.equals(env, "python3.8", get_lib_name_default(struct(basename = "libpython3.8.a")))

    asserts.equals(env, "python38", get_lib_name_for_windows(struct(basename = "python38.dll")))
    asserts.equals(env, "python38m", get_lib_name_for_windows(struct(basename = "python38m.dll")))

    asserts.equals(env, "python", get_lib_name_default(struct(basename = "libpython.so.3")))
    asserts.equals(env, "python", get_lib_name_default(struct(basename = "libpython.so.3.8")))
    asserts.equals(env, "python", get_lib_name_default(struct(basename = "libpython.so.3.8.0")))
    asserts.equals(env, "python", get_lib_name_default(struct(basename = "libpython.a.3")))
    asserts.equals(env, "python", get_lib_name_default(struct(basename = "libpython.a.3.8")))
    asserts.equals(env, "python", get_lib_name_default(struct(basename = "libpython.a.3.8.0")))
    asserts.equals(env, "python-3.8.0", get_lib_name_default(struct(basename = "libpython-3.8.0.so.3.8.0")))

    return unittest.end(env)

produced_expected_lib_name_test = unittest.make(_produced_expected_lib_name_test_impl)

def versioned_libs_unit_test_suite(name):
    """Unit tests for getting the link name of a versioned library.

    Args:
        name: the test suite name
    """
    unittest.suite(
        name,
        produced_expected_lib_name_test,
    )
