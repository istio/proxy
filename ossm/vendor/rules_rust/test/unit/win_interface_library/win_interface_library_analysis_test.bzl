"""Analysis tests for exporting the Windows interface library."""

load("@bazel_skylib//lib:unittest.bzl", "analysistest", "asserts")
load("@rules_cc//cc:defs.bzl", "cc_binary")
load("//rust:defs.bzl", "rust_binary", "rust_shared_library")

def _win_interface_library_test_impl(ctx):
    env = analysistest.begin(ctx)
    target = analysistest.target_under_test(env)

    files = target[DefaultInfo].files.to_list()
    cc_library = target[CcInfo].linking_context.linker_inputs.to_list()[0].libraries[0]

    # Make sure that we have both the `.dll` and the `.dll.lib` file in the default info's files
    asserts.equals(env, len(files), 2)
    asserts.true(env, files[0].basename.endswith(".dll"))
    asserts.true(env, files[1].basename.endswith(".dll.lib"))

    # Make sure that the cc_library has both a dynamic and interface library
    asserts.true(env, cc_library.dynamic_library != None)
    asserts.true(env, cc_library.interface_library != None)

    return analysistest.end(env)

win_interface_library_test = analysistest.make(_win_interface_library_test_impl)

def win_interface_library_analysis_test_suite(name):
    """Analysis tests for exporting the Windows interface library.

    Args:
        name: the test suite name
    """
    rust_shared_library(
        name = "mylib",
        srcs = ["lib.rs"],
        edition = "2018",
        target_compatible_with = ["@platforms//os:windows"],
    )

    cc_binary(
        name = "mybin",
        srcs = ["bin.cc"],
        deps = [":mylib"],
        target_compatible_with = ["@platforms//os:windows"],
    )

    rust_binary(
        name = "myrustbin",
        srcs = ["main.rs"],
        edition = "2018",
        target_compatible_with = ["@platforms//os:windows"],
    )

    win_interface_library_test(
        name = "win_interface_library_test",
        target_under_test = ":mylib",
        target_compatible_with = ["@platforms//os:windows"],
    )

    native.test_suite(
        name = name,
        tests = [":win_interface_library_test"],
    )
