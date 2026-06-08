"""Unit tests for crate names."""

load("@bazel_skylib//lib:unittest.bzl", "analysistest")
load("//rust:defs.bzl", "rust_binary", "rust_library", "rust_shared_library", "rust_static_library", "rust_test", "rust_test_suite")
load("//test/unit:common.bzl", "assert_argv_contains", "assert_argv_contains_prefix_not")

def _default_crate_name_library_test_impl(ctx):
    env = analysistest.begin(ctx)
    tut = analysistest.target_under_test(env)

    # Note: Hyphens in crate name converted to underscores.
    assert_argv_contains(env, tut.actions[0], "--crate-name=default_crate_name_library")
    return analysistest.end(env)

def _custom_crate_name_library_test_impl(ctx):
    env = analysistest.begin(ctx)
    tut = analysistest.target_under_test(env)
    assert_argv_contains(env, tut.actions[0], "--crate-name=custom_name")
    return analysistest.end(env)

def _default_crate_name_binary_test_impl(ctx):
    env = analysistest.begin(ctx)
    tut = analysistest.target_under_test(env)

    # Note: Hyphens in crate name converted to underscores.
    assert_argv_contains(env, tut.actions[0], "--crate-name=default_crate_name_binary")
    return analysistest.end(env)

def _custom_crate_name_binary_test_impl(ctx):
    env = analysistest.begin(ctx)
    tut = analysistest.target_under_test(env)
    assert_argv_contains(env, tut.actions[0], "--crate-name=custom_name")
    return analysistest.end(env)

def _default_crate_name_test_test_impl(ctx):
    env = analysistest.begin(ctx)
    tut = analysistest.target_under_test(env)

    # Note: Hyphens in crate name converted to underscores.
    assert_argv_contains(env, tut.actions[0], "--crate-name=default_crate_name_test")
    return analysistest.end(env)

def _custom_crate_name_test_test_impl(ctx):
    env = analysistest.begin(ctx)
    tut = analysistest.target_under_test(env)
    assert_argv_contains(env, tut.actions[0], "--crate-name=custom_name")
    return analysistest.end(env)

def _slib_library_name_test_impl(ctx):
    """Regression test for extra-filename.

    Checks that the extra hash value appended to the library filename only
    contains one dash. Previously, the hash for `slib` was negative,
    resulting in an extra dash in the filename (--codegen_extra_filename=--NUM).

    Args:
      ctx: rule context.
    """
    env = analysistest.begin(ctx)
    tut = analysistest.target_under_test(env)
    assert_argv_contains(env, tut.actions[0], "--codegen=metadata=-2102077805")
    assert_argv_contains(env, tut.actions[0], "--codegen=extra-filename=-2102077805")
    return analysistest.end(env)

def _no_extra_filename_test_impl(ctx):
    """Check that no extra filename is used.

    Args:
      ctx: rule context.
    """
    env = analysistest.begin(ctx)
    tut = analysistest.target_under_test(env)
    assert_argv_contains_prefix_not(env, tut.actions[0], "--codegen=metadata=")
    assert_argv_contains_prefix_not(env, tut.actions[0], "--codegen=extra-filename=")
    return analysistest.end(env)

def _default_crate_name_rust_test_suite_test_impl(ctx):
    env = analysistest.begin(ctx)
    tut = analysistest.target_under_test(env)

    # Note: Hyphens and Dots in crate name converted to underscores.
    assert_argv_contains(env, tut.actions[0], "--crate-name=default_crate_name_rust_test_suite_foo_bar_main_test")
    return analysistest.end(env)

default_crate_name_library_test = analysistest.make(
    _default_crate_name_library_test_impl,
)
custom_crate_name_library_test = analysistest.make(
    _custom_crate_name_library_test_impl,
)
default_crate_name_binary_test = analysistest.make(
    _default_crate_name_binary_test_impl,
)
custom_crate_name_binary_test = analysistest.make(
    _custom_crate_name_binary_test_impl,
)
default_crate_name_test_test = analysistest.make(
    _default_crate_name_test_test_impl,
)
custom_crate_name_test_test = analysistest.make(
    _custom_crate_name_test_test_impl,
)
slib_library_name_test = analysistest.make(
    _slib_library_name_test_impl,
)
no_extra_filename_test = analysistest.make(
    _no_extra_filename_test_impl,
)
default_crate_name_rust_test_suite_test = analysistest.make(
    _default_crate_name_rust_test_suite_test_impl,
)

def _crate_name_test():
    rust_library(
        name = "default/crate-name-library",
        srcs = ["lib.rs"],
        edition = "2018",
    )

    rust_library(
        name = "custom-crate-name-library",
        crate_name = "custom_name",
        srcs = ["lib.rs"],
        edition = "2018",
    )

    rust_binary(
        name = "default/crate-name-binary",
        srcs = ["main.rs"],
        edition = "2018",
    )

    rust_binary(
        name = "custom-crate-name-binary",
        crate_name = "custom_name",
        srcs = ["main.rs"],
        edition = "2018",
    )

    rust_binary(
        name = "custom_bin_target_name",
        crate_name = "smain",
        srcs = [
            "smain.rs",
            "add.rs",
        ],
        edition = "2018",
    )

    rust_test(
        name = "default/crate-name-test",
        srcs = ["main.rs"],
        edition = "2018",
    )

    rust_test(
        name = "custom-crate-name-test",
        crate_name = "custom_name",
        srcs = ["main.rs"],
        edition = "2018",
    )

    rust_test(
        name = "custom_named_test",
        crate_name = "stest",
        srcs = [
            "stest.rs",
            "add.rs",
        ],
        edition = "2018",
    )

    rust_library(
        name = "slib",
        srcs = ["slib.rs"],
        edition = "2018",
    )

    rust_library(
        name = "custom_lib_target_name",
        crate_name = "slib",
        srcs = [
            "slib.rs",
            "add.rs",
        ],
        edition = "2018",
    )

    rust_shared_library(
        name = "shared_lib",
        srcs = ["lib.rs"],
        edition = "2018",
    )

    rust_static_library(
        name = "static_lib",
        srcs = ["lib.rs"],
        edition = "2018",
    )

    rust_test_suite(
        name = "default/crate-name-rust-test-suite",
        srcs = ["foo.bar.main.rs"],
        edition = "2018",
    )

    slib_library_name_test(
        name = "slib_library_name_test",
        target_under_test = ":slib",
    )

    default_crate_name_library_test(
        name = "default_crate_name_library_test",
        target_under_test = ":default/crate-name-library",
    )

    custom_crate_name_library_test(
        name = "custom_crate_name_library_test",
        target_under_test = ":custom-crate-name-library",
    )

    default_crate_name_binary_test(
        name = "default_crate_name_binary_test",
        target_under_test = ":default/crate-name-binary",
    )

    custom_crate_name_binary_test(
        name = "custom_crate_name_binary_test",
        target_under_test = ":custom-crate-name-binary",
    )

    default_crate_name_test_test(
        name = "default_crate_name_test_test",
        target_under_test = ":default/crate-name-test",
    )

    custom_crate_name_test_test(
        name = "custom_crate_name_test_test",
        target_under_test = ":custom-crate-name-test",
    )

    no_extra_filename_test(
        name = "no_extra_filename_for_shared_library_test",
        target_under_test = ":shared_lib",
    )

    no_extra_filename_test(
        name = "no_extra_filename_for_static_library_test",
        target_under_test = ":static_lib",
    )
    default_crate_name_rust_test_suite_test(
        name = "default_crate_name_rust_test_suite_test",
        target_under_test = ":default/crate-name-rust-test-suite_foo.bar.main_test",
    )

def crate_name_test_suite(name):
    """Entry-point macro called from the BUILD file.

    Args:
        name: Name of the macro.
    """

    _crate_name_test()

    native.test_suite(
        name = name,
        tests = [
            ":default_crate_name_library_test",
            ":custom_crate_name_library_test",
            ":default_crate_name_binary_test",
            ":custom_crate_name_binary_test",
            ":default_crate_name_test_test",
            ":custom_crate_name_test_test",
            ":default_crate_name_rust_test_suite_test",
        ],
    )
