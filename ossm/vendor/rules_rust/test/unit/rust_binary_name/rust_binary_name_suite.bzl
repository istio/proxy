"""Starlark tests for `rust_binary.binary_name`"""

load("@bazel_skylib//lib:paths.bzl", "paths")
load("@bazel_skylib//lib:unittest.bzl", "analysistest", "asserts")
load("@bazel_skylib//rules:write_file.bzl", "write_file")
load("//rust:defs.bzl", "rust_binary")

def _rust_binary_binary_name_test_impl(ctx):
    expected_basename = ctx.attr.expected_binary_name

    env = analysistest.begin(ctx)
    target = analysistest.target_under_test(env)

    action = target.actions[0]
    output = action.outputs.to_list()[0]

    filename = paths.split_extension(output.basename)[0]
    asserts.equals(env, filename, expected_basename)

    return analysistest.end(env)

_binary_name_test = analysistest.make(
    _rust_binary_binary_name_test_impl,
    attrs = {
        "expected_binary_name": attr.string(),
    },
)

def binary_name_test_suite(name):
    """Entry-point macro called from the BUILD file.

    Args:
        name (str): The name of the test suite.
    """
    write_file(
        name = "main",
        out = "main.rs",
        content = [
            "fn main() {}",
            "",
        ],
    )

    rust_binary(
        name = "bin_unset",
        srcs = [":main.rs"],
        edition = "2021",
    )

    _binary_name_test(
        name = "unset_binary_name_test",
        target_under_test = ":bin_unset",
        expected_binary_name = "bin_unset",
    )

    rust_binary(
        name = "bin",
        binary_name = "some-binary",
        srcs = [":main.rs"],
        edition = "2021",
    )

    _binary_name_test(
        name = "set_binary_name_test",
        target_under_test = ":bin",
        expected_binary_name = "some-binary",
    )

    native.test_suite(
        name = name,
        tests = [
            ":unset_binary_name_test",
            ":set_binary_name_test",
        ],
    )
