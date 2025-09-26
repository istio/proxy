"""Analysis tests for debug info in cdylib and bin targets."""

load("@bazel_skylib//lib:unittest.bzl", "analysistest", "asserts")
load("//rust:defs.bzl", "rust_binary", "rust_shared_library", "rust_test")

def _pdb_file_test_impl(ctx, expect_pdb_file):
    env = analysistest.begin(ctx)
    target = analysistest.target_under_test(env)
    files = target[DefaultInfo].files.to_list()

    if not expect_pdb_file:
        asserts.equals(env, len(files), 0)
        return analysistest.end(env)

    asserts.equals(env, len(files), 1)
    file = files[0]
    asserts.equals(env, file.extension, "pdb")
    return analysistest.end(env)

def _pdb_file_for_dbg_test_impl(ctx):
    """Test for dbg compilation mode."""
    return _pdb_file_test_impl(ctx, True)

pdb_file_dbg_test = analysistest.make(
    _pdb_file_for_dbg_test_impl,
    config_settings = {
        "//command_line_option:compilation_mode": "dbg",
    },
)

def _pdb_file_for_fastbuild_test_impl(ctx):
    """Test for fastbuild compilation mode."""
    return _pdb_file_test_impl(ctx, True)

pdb_file_fastbuild_test = analysistest.make(
    _pdb_file_for_fastbuild_test_impl,
    config_settings = {
        "//command_line_option:compilation_mode": "fastbuild",
    },
)

def _pdb_file_for_opt_test_impl(ctx):
    """Test for opt compilation mode."""
    return _pdb_file_test_impl(ctx, False)

pdb_file_opt_test = analysistest.make(
    _pdb_file_for_opt_test_impl,
    config_settings = {
        "//command_line_option:compilation_mode": "opt",
    },
)

# Mapping from compilation mode to pdb file test.
pdb_file_tests = {
    "dbg": pdb_file_dbg_test,
    "fastbuild": pdb_file_fastbuild_test,
    "opt": pdb_file_opt_test,
}

def _dsym_folder_test_impl(ctx):
    env = analysistest.begin(ctx)
    target = analysistest.target_under_test(env)

    files = target[DefaultInfo].files.to_list()
    asserts.equals(env, len(files), 1)
    file = files[0]
    asserts.equals(env, file.extension, "dSYM")

    return analysistest.end(env)

dsym_folder_test = analysistest.make(_dsym_folder_test_impl)

def debug_info_analysis_test_suite(name):
    """Analysis tests for debug info in cdylib and bin targets.

    Args:
        name: the test suite name
    """
    rust_shared_library(
        name = "mylib",
        srcs = ["lib.rs"],
        edition = "2018",
    )

    native.filegroup(
        name = "mylib.pdb",
        srcs = [":mylib"],
        output_group = "pdb_file",
    )

    for compilation_mode, pdb_test in pdb_file_tests.items():
        pdb_test(
            name = "lib_pdb_test_{}".format(compilation_mode),
            target_under_test = ":mylib.pdb",
            target_compatible_with = ["@platforms//os:windows"],
        )

    native.filegroup(
        name = "mylib.dSYM",
        srcs = [":mylib"],
        output_group = "dsym_folder",
    )

    dsym_folder_test(
        name = "lib_dsym_test",
        target_under_test = ":mylib.dSYM",
        target_compatible_with = ["@platforms//os:macos"],
    )

    rust_binary(
        name = "myrustbin",
        srcs = ["main.rs"],
        edition = "2018",
    )

    native.filegroup(
        name = "mybin.pdb",
        srcs = [":myrustbin"],
        output_group = "pdb_file",
    )

    for compilation_mode, pdb_test in pdb_file_tests.items():
        pdb_test(
            name = "bin_pdb_test_{}".format(compilation_mode),
            target_under_test = ":mybin.pdb",
            target_compatible_with = ["@platforms//os:windows"],
        )

    native.filegroup(
        name = "mybin.dSYM",
        srcs = [":myrustbin"],
        output_group = "dsym_folder",
    )

    dsym_folder_test(
        name = "bin_dsym_test",
        target_under_test = ":mybin.dSYM",
        target_compatible_with = ["@platforms//os:macos"],
    )

    rust_test(
        name = "myrusttest",
        srcs = ["test.rs"],
        edition = "2018",
    )

    native.filegroup(
        name = "mytest.pdb",
        srcs = [":myrusttest"],
        output_group = "pdb_file",
        testonly = True,
    )

    for compilation_mode, pdb_test in pdb_file_tests.items():
        pdb_test(
            name = "test_pdb_test_{}".format(compilation_mode),
            target_under_test = ":mytest.pdb",
            target_compatible_with = ["@platforms//os:windows"],
        )

    native.filegroup(
        name = "mytest.dSYM",
        srcs = [":myrusttest"],
        output_group = "dsym_folder",
        testonly = True,
    )

    dsym_folder_test(
        name = "test_dsym_test",
        target_under_test = ":mytest.dSYM",
        target_compatible_with = ["@platforms//os:macos"],
    )

    native.test_suite(
        name = name,
        tests = [
            ":lib_dsym_test",
            ":bin_dsym_test",
            ":test_dsym_test",
        ] + [
            ":lib_pdb_test_{}".format(compilation_mode)
            for compilation_mode in pdb_file_tests
        ] + [
            ":bin_pdb_test_{}".format(compilation_mode)
            for compilation_mode in pdb_file_tests
        ] + [
            ":test_pdb_test_{}".format(compilation_mode)
            for compilation_mode in pdb_file_tests
        ],
    )
