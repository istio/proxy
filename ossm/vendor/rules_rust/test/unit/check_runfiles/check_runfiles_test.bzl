"""Unittests for rust rules."""

load("@bazel_skylib//lib:unittest.bzl", "analysistest", "asserts")
load(
    "//rust:defs.bzl",
    "rust_binary",
    "rust_library",
    "rust_shared_library",
    "rust_static_library",
)

def _check_runfiles_test_impl(ctx):
    env = analysistest.begin(ctx)
    tut = analysistest.target_under_test(env)
    runfiles = tut[DefaultInfo].default_runfiles.files.to_list()

    asserts.true(env, _is_in_runfiles("libbar.so", runfiles))

    return analysistest.end(env)

def _is_in_runfiles(name, runfiles):
    for file in runfiles:
        if file.basename == name:
            return True
    return False

check_runfiles_test = analysistest.make(_check_runfiles_test_impl)

def _check_runfiles_test():
    rust_library(
        name = "foo_lib",
        srcs = ["foo.rs"],
        edition = "2018",
        deps = [":libbar.so"],
    )

    rust_binary(
        name = "foo_bin",
        srcs = ["foo_main.rs"],
        edition = "2018",
        deps = [":libbar.so"],
    )

    rust_shared_library(
        name = "foo_dylib",
        srcs = ["foo.rs"],
        edition = "2018",
        deps = [":libbar.so"],
    )

    rust_static_library(
        name = "foo_static",
        srcs = ["foo.rs"],
        edition = "2018",
        deps = [":libbar.so"],
    )

    # buildifier: disable=native-cc
    native.cc_binary(
        name = "libbar.so",
        srcs = ["bar.cc"],
        linkshared = True,
    )

    check_runfiles_test(
        name = "check_runfiles_lib_test",
        target_under_test = ":foo_lib",
    )

    check_runfiles_test(
        name = "check_runfiles_bin_test",
        target_under_test = ":foo_bin",
    )

    check_runfiles_test(
        name = "check_runfiles_dylib_test",
        target_under_test = ":foo_dylib",
    )

    check_runfiles_test(
        name = "check_runfiles_static_test",
        target_under_test = ":foo_static",
    )

def check_runfiles_test_suite(name):
    """Entry-point macro called from the BUILD file.

    Args:
        name: Name of the macro.
    """
    _check_runfiles_test()

    native.test_suite(
        name = name,
        tests = [
            ":check_runfiles_lib_test",
            ":check_runfiles_bin_test",
            ":check_runfiles_dylib_test",
            ":check_runfiles_static_test",
        ],
    )
