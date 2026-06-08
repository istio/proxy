"""collect_cfgs_test_suite"""

load("@bazel_skylib//lib:unittest.bzl", "analysistest", "asserts")
load("//rust:defs.bzl", "rust_library", "rust_proc_macro")
load("//rust:rust_common.bzl", "CrateInfo")

def _collect_cfgs_test(ctx):
    env = analysistest.begin(ctx)

    target_under_test = analysistest.target_under_test(env)

    expected = sorted(ctx.attr.expect_cfgs)
    actual = sorted(target_under_test[CrateInfo].cfgs)
    asserts.equals(env, expected, actual)

    return analysistest.end(env)

collect_cfgs_test = analysistest.make(
    _collect_cfgs_test,
    attrs = {
        "expect_cfgs": attr.string_list(),
    },
    config_settings = {
        str(Label("//rust/settings:collect_cfgs")): True,
    },
)

collect_cfgs_disabled_test = analysistest.make(
    _collect_cfgs_test,
    attrs = {
        "expect_cfgs": attr.string_list(default = []),
    },
    config_settings = {
        str(Label("//rust/settings:collect_cfgs")): False,
    },
)

collect_cfgs_no_std_test = analysistest.make(
    _collect_cfgs_test,
    attrs = {
        "expect_cfgs": attr.string_list(default = ['feature="no_std"']),
    },
    config_settings = {
        str(Label("//rust/settings:collect_cfgs")): True,
        str(Label("//rust/settings:no_std")): "alloc",
    },
)

collect_cfgs_extra_rustc_flag_test = analysistest.make(
    _collect_cfgs_test,
    attrs = {
        "expect_cfgs": attr.string_list(default = ["foo"]),
    },
    config_settings = {
        str(Label("//rust/settings:collect_cfgs")): True,
        str(Label("//rust/settings:extra_rustc_flag")): ["--cfg=foo"],
    },
)

collect_cfgs_extra_rustc_flags_test = analysistest.make(
    _collect_cfgs_test,
    attrs = {
        "expect_cfgs": attr.string_list(default = ["bar", "baz"]),
    },
    config_settings = {
        str(Label("//rust/settings:collect_cfgs")): True,
        str(Label("//rust/settings:extra_rustc_flags")): ["--cfg=bar", "--cfg=baz"],
    },
)

collect_cfgs_per_crate_rustc_flag_test = analysistest.make(
    _collect_cfgs_test,
    attrs = {
        "expect_cfgs": attr.string_list(default = ["foo"]),
    },
    config_settings = {
        str(Label("//rust/settings:collect_cfgs")): True,
        str(Label("//rust/settings:experimental_per_crate_rustc_flag")): ["//test/unit@--cfg=foo"],
    },
)

def _collect_cfgs_exec_test(ctx):
    env = analysistest.begin(ctx)

    # Get the CrateInfo provider of the single proc macro dependency that the target under test should have.
    target_under_test = analysistest.target_under_test(env)
    proc_macro_deps = target_under_test[CrateInfo].proc_macro_deps.to_list()
    proc_macro_crate_info = proc_macro_deps[0].crate_info

    expected = sorted(ctx.attr.expect_cfgs)
    actual = sorted(proc_macro_crate_info.cfgs)
    asserts.equals(env, expected, actual)

    return analysistest.end(env)

collect_cfgs_extra_exec_rustc_flag_test = analysistest.make(
    _collect_cfgs_exec_test,
    attrs = {
        "expect_cfgs": attr.string_list(default = ["foo"]),
    },
    config_settings = {
        str(Label("//rust/settings:collect_cfgs")): True,
        str(Label("//rust/settings:extra_exec_rustc_flag")): ["--cfg=foo"],
    },
)

collect_cfgs_extra_exec_rustc_flags_test = analysistest.make(
    _collect_cfgs_exec_test,
    attrs = {
        "expect_cfgs": attr.string_list(default = ["bar", "baz"]),
    },
    config_settings = {
        str(Label("//rust/settings:collect_cfgs")): True,
        str(Label("//rust/settings:extra_exec_rustc_flags")): ["--cfg=bar", "--cfg=baz"],
    },
)

def collect_cfgs_test_suite(name):
    """Tests for the `//rust/settings:collect_cfgs` settings.

    Args:
        name (str): The name of the test suite.
    """
    rust_library(
        name = "lib",
        srcs = ["lib.rs"],
        edition = "2018",
    )

    rust_proc_macro(
        name = "proc_macro",
        srcs = ["lib.rs"],
        edition = "2018",
    )

    rust_library(
        name = "lib_with_proc_macro_dep",
        srcs = ["lib.rs"],
        proc_macro_deps = [":proc_macro"],
        edition = "2018",
    )

    rust_library(
        name = "lib_with_crate_features",
        srcs = ["lib.rs"],
        crate_features = ["foo", "bar"],
        edition = "2018",
    )

    rust_library(
        name = "lib_with_rustc_flags",
        srcs = ["lib.rs"],
        rustc_flags = ["--cfg=baz"],
        edition = "2018",
    )

    collect_cfgs_test(
        name = "collect_cfgs_crate_features_test",
        target_under_test = ":lib_with_crate_features",
        expect_cfgs = ['feature="foo"', 'feature="bar"'],
    )

    collect_cfgs_test(
        name = "collect_cfgs_rustc_flags_test",
        target_under_test = ":lib_with_rustc_flags",
        expect_cfgs = ["baz"],
    )

    collect_cfgs_disabled_test(
        name = "collect_cfgs_disabled_test",
        target_under_test = ":lib_with_crate_features",
    )

    collect_cfgs_no_std_test(
        name = "collect_cfgs_no_std_test",
        target_under_test = ":lib",
    )

    collect_cfgs_extra_rustc_flag_test(
        name = "collect_cfgs_extra_rustc_flag_test",
        target_under_test = ":lib",
    )

    collect_cfgs_extra_rustc_flags_test(
        name = "collect_cfgs_extra_rustc_flags_test",
        target_under_test = ":lib",
    )

    collect_cfgs_per_crate_rustc_flag_test(
        name = "collect_cfgs_per_crate_rustc_flag_test",
        target_under_test = ":lib",
    )

    collect_cfgs_extra_exec_rustc_flag_test(
        name = "collect_cfgs_extra_exec_rustc_flag_test",
        target_under_test = ":lib_with_proc_macro_dep",
    )

    collect_cfgs_extra_exec_rustc_flags_test(
        name = "collect_cfgs_extra_exec_rustc_flags_test",
        target_under_test = ":lib_with_proc_macro_dep",
    )

    native.test_suite(
        name = name,
        tests = [
            ":collect_cfgs_crate_features_test",
            ":collect_cfgs_rustc_flags_test",
            ":collect_cfgs_disabled_test",
            ":collect_cfgs_no_std_test",
            ":collect_cfgs_extra_rustc_flag_test",
            ":collect_cfgs_extra_rustc_flags_test",
            ":collect_cfgs_per_crate_rustc_flag_test",
            ":collect_cfgs_extra_exec_rustc_flag_test",
            ":collect_cfgs_extra_exec_rustc_flags_test",
        ],
    )
