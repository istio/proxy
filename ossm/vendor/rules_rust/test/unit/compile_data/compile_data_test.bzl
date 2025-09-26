"""Unittest to verify compile_data (attribute) propagation"""

load("@bazel_skylib//lib:unittest.bzl", "analysistest", "asserts")
load("//rust:defs.bzl", "rust_common", "rust_doc", "rust_library", "rust_test")
load(
    "//test/unit:common.bzl",
    "assert_action_mnemonic",
)

def _target_has_compile_data(ctx, expected):
    env = analysistest.begin(ctx)
    target = analysistest.target_under_test(env)

    # Extract compile_data from a target expected to have a `CrateInfo` provider
    crate_info = target[rust_common.crate_info]
    compile_data = crate_info.compile_data.to_list()

    # Ensure compile data was correctly propagated to the provider
    asserts.equals(
        env,
        sorted([data.short_path for data in compile_data]),
        expected,
    )

    return analysistest.end(env)

def _compile_data_propagates_to_crate_info_test_impl(ctx):
    return _target_has_compile_data(
        ctx,
        ["test/unit/compile_data/compile_data.txt"],
    )

def _wrapper_rule_propagates_to_crate_info_test_impl(ctx):
    return _target_has_compile_data(
        ctx,
        ["test/unit/compile_data/compile_data.txt"],
    )

def _wrapper_rule_propagates_and_joins_compile_data_test_impl(ctx):
    return _target_has_compile_data(
        ctx,
        [
            "test/unit/compile_data/compile_data.txt",
            "test/unit/compile_data/test_compile_data.txt",
        ],
    )

def _compile_data_propagates_to_rust_doc_test_impl(ctx):
    env = analysistest.begin(ctx)
    target = analysistest.target_under_test(env)

    actions = target.actions
    action = actions[0]
    assert_action_mnemonic(env, action, "Rustdoc")

    return analysistest.end(env)

compile_data_propagates_to_crate_info_test = analysistest.make(_compile_data_propagates_to_crate_info_test_impl)
wrapper_rule_propagates_to_crate_info_test = analysistest.make(_wrapper_rule_propagates_to_crate_info_test_impl)
wrapper_rule_propagates_and_joins_compile_data_test = analysistest.make(_wrapper_rule_propagates_and_joins_compile_data_test_impl)
compile_data_propagates_to_rust_doc_test = analysistest.make(_compile_data_propagates_to_rust_doc_test_impl)

def _define_test_targets():
    rust_library(
        name = "compile_data",
        srcs = ["compile_data.rs"],
        compile_data = ["compile_data.txt"],
        edition = "2018",
    )

    rust_test(
        name = "compile_data_unit_test",
        crate = ":compile_data",
    )

    rust_test(
        name = "test_compile_data_unit_test",
        compile_data = ["test_compile_data.txt"],
        crate = ":compile_data",
        rustc_flags = ["--cfg=test_compile_data"],
    )

    rust_library(
        name = "compile_data_env",
        srcs = ["compile_data_env.rs"],
        compile_data = ["compile_data.txt"],
        rustc_env = {
            "COMPILE_DATA_PATH": "$(execpath :compile_data.txt)",
        },
        edition = "2018",
    )

    rust_doc(
        name = "compile_data_env_rust_doc",
        crate = ":compile_data_env",
    )

def compile_data_test_suite(name):
    """Entry-point macro called from the BUILD file.

    Args:
        name (str): Name of the macro.
    """

    _define_test_targets()

    compile_data_propagates_to_crate_info_test(
        name = "compile_data_propagates_to_crate_info_test",
        target_under_test = ":compile_data",
    )

    wrapper_rule_propagates_to_crate_info_test(
        name = "wrapper_rule_propagates_to_crate_info_test",
        target_under_test = ":compile_data_unit_test",
    )

    wrapper_rule_propagates_and_joins_compile_data_test(
        name = "wrapper_rule_propagates_and_joins_compile_data_test",
        target_under_test = ":test_compile_data_unit_test",
    )

    compile_data_propagates_to_rust_doc_test(
        name = "compile_data_propagates_to_rust_doc_test",
        target_under_test = ":compile_data_env_rust_doc",
    )

    native.test_suite(
        name = name,
        tests = [
            ":compile_data_propagates_to_crate_info_test",
            ":wrapper_rule_propagates_to_crate_info_test",
            ":wrapper_rule_propagates_and_joins_compile_data_test",
            ":compile_data_propagates_to_rust_doc_test",
        ],
    )
