"""Unittests for rust rules."""

load("@bazel_skylib//lib:unittest.bzl", "analysistest", "asserts")
load("//rust:defs.bzl", "rust_common", "rust_library")

def _crate_variants_test_impl(ctx):
    env = analysistest.begin(ctx)
    tut = analysistest.target_under_test(env)
    transitive_crate_outputs = tut[rust_common.dep_info].transitive_crate_outputs.to_list()

    # Both variants of "foo" occur as dependencies.
    asserts.equals(env, len(transitive_crate_outputs), 2)
    return analysistest.end(env)

crate_variants_test = analysistest.make(_crate_variants_test_impl)

def _crate_variants_test():
    rust_library(
        name = "foo",
        srcs = ["foo.rs"],
        edition = "2018",
    )

    rust_library(
        name = "foo2",
        crate_name = "foo",
        srcs = ["foo.rs"],
        edition = "2018",
    )

    rust_library(
        name = "bar",
        srcs = ["bar.rs"],
        edition = "2018",
        deps = [":foo", ":foo2"],
    )

    crate_variants_test(
        name = "crate_variants_test",
        target_under_test = ":bar",
    )

def crate_variants_test_suite(name):
    """Entry-point macro called from the BUILD file.

    Args:
        name: Name of the macro.
    """
    _crate_variants_test()

    native.test_suite(
        name = name,
        tests = [
            ":crate_variants_test",
        ],
    )
