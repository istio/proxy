"""Unittests for rust rules."""

load("@bazel_skylib//lib:unittest.bzl", "analysistest", "asserts")
load("//rust:defs.bzl", "rust_common", "rust_library")

def _transitive_crate_outputs_test_impl(ctx):
    env = analysistest.begin(ctx)
    tut = analysistest.target_under_test(env)
    transitive_crate_outputs = tut[rust_common.dep_info].transitive_crate_outputs.to_list()

    # Check that the non-crate output baz.a didn't find its way into transitive_crate_outputs
    # while the bar.rlib did.
    asserts.equals(env, len(transitive_crate_outputs), 1)
    asserts.equals(env, transitive_crate_outputs[0].extension, "rlib")

    return analysistest.end(env)

transitive_crate_outputs_test = analysistest.make(_transitive_crate_outputs_test_impl)

def _transitive_crate_outputs_test():
    rust_library(
        name = "foo",
        srcs = ["foo.rs"],
        edition = "2018",
        deps = [":bar", ":baz"],
    )

    rust_library(
        name = "bar",
        srcs = ["bar.rs"],
        edition = "2018",
    )

    # buildifier: disable=native-cc
    native.cc_library(
        name = "baz",
        srcs = ["baz.cc"],
    )

    transitive_crate_outputs_test(
        name = "transitive_crate_outputs_test",
        target_under_test = ":foo",
    )

def transitive_crate_outputs_test_suite(name):
    """Entry-point macro called from the BUILD file.

    Args:
        name: Name of the macro.
    """
    _transitive_crate_outputs_test()

    native.test_suite(
        name = name,
        tests = [
            ":transitive_crate_outputs_test",
        ],
    )
