"""Tests for rust_test outputs directory."""

load("@bazel_skylib//lib:unittest.bzl", "analysistest", "asserts")
load("//rust:defs.bzl", "rust_binary", "rust_common", "rust_test")

def _rust_test_outputs_test(ctx):
    env = analysistest.begin(ctx)
    tut = analysistest.target_under_test(env)

    output = tut[rust_common.crate_info].output

    asserts.true(env, output.dirname.split("/")[-1].startswith("test-"))

    return analysistest.end(env)

rust_test_outputs_test = analysistest.make(
    _rust_test_outputs_test,
)

def _rust_test_outputs_targets():
    rust_binary(
        name = "bin_outputs",
        srcs = ["foo.rs"],
        edition = "2018",
    )

    rust_test(
        name = "test_outputs_with_srcs",
        srcs = ["foo.rs"],
        edition = "2018",
    )

    rust_test_outputs_test(
        name = "rust_test_outputs_using_srcs_attr",
        target_under_test = ":test_outputs_with_srcs",
    )

    rust_test(
        name = "test_outputs_with_crate",
        crate = "bin_outputs",
        edition = "2018",
    )

    rust_test_outputs_test(
        name = "rust_test_outputs_using_crate_attr",
        target_under_test = ":test_outputs_with_crate",
    )

def rust_test_outputs_test_suite(name):
    """Entry-point macro called from the BUILD file.

    Args:
        name: Name of the macro.
    """

    _rust_test_outputs_targets()

    native.test_suite(
        name = name,
        tests = [
            ":rust_test_outputs_using_srcs_attr",
            ":rust_test_outputs_using_crate_attr",
        ],
    )
