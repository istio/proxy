"""Analysis tests for the name we assign to cdylib libraries."""

load("@bazel_skylib//lib:unittest.bzl", "analysistest", "asserts")
load("//rust:defs.bzl", "rust_shared_library")

def _cdylib_name_test_impl(ctx):
    env = analysistest.begin(ctx)
    target = analysistest.target_under_test(env)

    # We're expecting the `.dylib`/`.so` to be the only file on Unix and Windows to
    # contain a pair of `.dll` and `.lib` files.
    files = target[DefaultInfo].files.to_list()
    if len(files) == 1:
        asserts.true(env, files[0].extension in ("so", "dylib"))
        if files[0].extension == "so":
            asserts.equals(env, files[0].basename, "libsomething.so")
        elif files[0].extension == "dylib":
            asserts.equals(env, files[0].basename, "libsomething.dylib")
    elif len(files) == 2:
        expected_filenames = ["something.dll", "something.dll.lib"]
        for file in files:
            asserts.true(env, file.basename in expected_filenames)
            expected_filenames.remove(file.basename)

    return analysistest.end(env)

cdylib_name_test = analysistest.make(_cdylib_name_test_impl)

def cdylib_name_analysis_test_suite(name):
    """Analysis tests for the name we assign to cdylib libraries.

    Args:
        name: the test suite name
    """
    rust_shared_library(
        name = "something",
        srcs = ["lib.rs"],
        edition = "2018",
    )

    cdylib_name_test(
        name = "cdylib_name_test",
        target_under_test = ":something",
    )

    native.test_suite(
        name = name,
        tests = [":cdylib_name_test"],
    )
