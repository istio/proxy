"""Unittests for rust rules."""

load("@bazel_skylib//lib:unittest.bzl", "analysistest", "asserts")
load("//rust:defs.bzl", "rust_binary", "rust_library", "rust_proc_macro", "rust_test")

def _metadata_output_groups_present_test_impl(ctx):
    env = analysistest.begin(ctx)
    tut = analysistest.target_under_test(env)

    output_groups = tut[OutputGroupInfo]
    build_metadata = output_groups.build_metadata.to_list()
    rustc_rmeta_output = output_groups.rustc_rmeta_output.to_list()

    asserts.equals(env, 1, len(build_metadata), "Expected 1 build_metadata file")
    asserts.true(
        build_metadata[0].basename.endswith(".rmeta"),
        "Expected %s to end with .rmeta" % build_metadata[0],
    )

    asserts.equals(env, 1, len(rustc_rmeta_output), "Expected 1 rustc_rmeta_output file")
    asserts.true(
        rustc_rmeta_output[0].basename.endswith(".rustc-output"),
        "Expected %s to end with .rustc-output" % rustc_rmeta_output[0],
    )

    return analysistest.end(env)

def _metadata_output_groups_missing_test_impl(ctx):
    env = analysistest.begin(ctx)
    tut = analysistest.target_under_test(env)

    output_groups = tut[OutputGroupInfo]
    asserts.false(env, hasattr(output_groups, "build_metadata"), "Expected no build_metadata output group")
    asserts.false(env, hasattr(output_groups, "rustc_rmeta_output"), "Expected no rustc_rmeta_output output group")

    return analysistest.end(env)

metadata_output_groups_present_test = analysistest.make(
    _metadata_output_groups_present_test_impl,
    config_settings = {
        str(Label("//rust/settings:always_enable_metadata_output_groups")): True,
        str(Label("//rust/settings:rustc_output_diagnostics")): True,
    },
)

metadata_output_groups_missing_test = analysistest.make(
    _metadata_output_groups_missing_test_impl,
)

def _output_groups_test(*, always_enable):
    suffix = "_with_metadata" if always_enable else "_without_metadata"

    rust_binary(
        name = "bin" + suffix,
        srcs = ["bin.rs"],
        edition = "2021",
    )

    rust_library(
        name = "lib" + suffix,
        srcs = ["lib.rs"],
        edition = "2021",
    )

    rust_proc_macro(
        name = "macro" + suffix,
        srcs = ["macro.rs"],
        edition = "2021",
    )

    rust_test(
        name = "unit" + suffix,
        srcs = ["unit.rs"],
        edition = "2021",
    )

    if always_enable:
        test = metadata_output_groups_present_test
    else:
        test = metadata_output_groups_missing_test

    test(
        name = "bin_test" + suffix,
        target_under_test = ":bin" + suffix,
    )

    test(
        name = "lib_test" + suffix,
        target_under_test = ":lib" + suffix,
    )

    test(
        name = "macro_test" + suffix,
        target_under_test = ":macro" + suffix,
    )

    test(
        name = "unit_test" + suffix,
        target_under_test = ":unit" + suffix,
    )

    return [
        ":bin_test" + suffix,
        ":lib_test" + suffix,
        ":macro_test" + suffix,
        ":unit_test" + suffix,
    ]

def metadata_output_groups_test_suite(name):
    """Entry-point macro called from the BUILD file.

    Args:
        name: Name of the macro.
    """
    tests = []
    tests.extend(_output_groups_test(always_enable = True))
    tests.extend(_output_groups_test(always_enable = False))

    native.test_suite(
        name = name,
        tests = tests,
    )
