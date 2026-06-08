"""
Tests for handling of cc_toolchain's static_runtime_lib/dynamic_runtime_lib.
"""

load("@bazel_skylib//lib:unittest.bzl", "analysistest", "asserts")
load("@rules_cc//cc:cc_toolchain_config_lib.bzl", "feature")
load("@rules_cc//cc:defs.bzl", "cc_toolchain")
load("@rules_cc//cc/common:cc_common.bzl", "cc_common")
load("@rules_cc//cc/toolchains:cc_toolchain_config_info.bzl", "CcToolchainConfigInfo")
load("//rust:defs.bzl", "rust_shared_library", "rust_static_library")

def _test_cc_config_impl(ctx):
    config_info = cc_common.create_cc_toolchain_config_info(
        ctx = ctx,
        toolchain_identifier = "test-cc-toolchain",
        host_system_name = "unknown",
        target_system_name = "unknown",
        target_cpu = "unknown",
        target_libc = "unknown",
        compiler = "unknown",
        abi_version = "unknown",
        abi_libc_version = "unknown",
        features = [
            feature(name = "static_link_cpp_runtimes", enabled = True),
        ],
    )
    return config_info

test_cc_config = rule(
    implementation = _test_cc_config_impl,
    provides = [CcToolchainConfigInfo],
)

def _with_extra_toolchain_transition_impl(_settings, attr):
    return {"//command_line_option:extra_toolchains": [attr.extra_toolchain]}

with_extra_toolchain_transition = transition(
    implementation = _with_extra_toolchain_transition_impl,
    inputs = [],
    outputs = ["//command_line_option:extra_toolchains"],
)

DepActionsInfo = provider(
    "Contains information about dependencies actions.",
    fields = {"actions": "List[Action]"},
)

def _with_extra_toolchain_impl(ctx):
    return [
        DepActionsInfo(actions = ctx.attr.target[0].actions),
    ]

with_extra_toolchain = rule(
    implementation = _with_extra_toolchain_impl,
    attrs = {
        "extra_toolchain": attr.label(),
        "target": attr.label(cfg = with_extra_toolchain_transition),
    },
)

def _inputs_analysis_test_impl(ctx):
    env = analysistest.begin(ctx)
    tut = analysistest.target_under_test(env)
    action = tut[DepActionsInfo].actions[0]
    asserts.equals(env, action.mnemonic, "Rustc")
    inputs = action.inputs.to_list()
    for expected in ctx.attr.expected_inputs:
        asserts.true(
            env,
            any([input.path.endswith("/" + expected) for input in inputs]),
            "error: expected '{}' to be in inputs: '{}'".format(expected, inputs),
        )

    return analysistest.end(env)

inputs_analysis_test = analysistest.make(
    impl = _inputs_analysis_test_impl,
    doc = """An analysistest to examine the inputs of a library target.""",
    attrs = {
        "expected_inputs": attr.string_list(),
    },
)

def runtime_libs_test(name):
    """Produces test shared and static library targets that are set up to use a custom cc_toolchain with custom runtime libs.

    Args:
      name: The name of the test target.
    """

    test_cc_config(
        name = "%s/cc_toolchain_config" % name,
    )
    cc_toolchain(
        name = "%s/test_cc_toolchain_impl" % name,
        all_files = ":empty",
        compiler_files = ":empty",
        dwp_files = ":empty",
        linker_files = ":empty",
        objcopy_files = ":empty",
        strip_files = ":empty",
        supports_param_files = 0,
        toolchain_config = ":%s/cc_toolchain_config" % name,
        toolchain_identifier = "dummy_wasm32_cc",
        static_runtime_lib = ":dummy.a",
        dynamic_runtime_lib = ":dummy.so",
    )
    native.toolchain(
        name = "%s/test_cc_toolchain" % name,
        toolchain = ":%s/test_cc_toolchain_impl" % name,
        toolchain_type = "@bazel_tools//tools/cpp:toolchain_type",
    )

    rust_shared_library(
        name = "%s/__shared_library" % name,
        edition = "2018",
        srcs = ["lib.rs"],
        tags = ["manual", "nobuild"],
    )

    with_extra_toolchain(
        name = "%s/_shared_library" % name,
        extra_toolchain = ":%s/test_cc_toolchain" % name,
        target = "%s/__shared_library" % name,
        tags = ["manual"],
    )

    inputs_analysis_test(
        name = "%s/shared_library" % name,
        target_under_test = "%s/_shared_library" % name,
        expected_inputs = ["dummy.so"],
    )

    rust_static_library(
        name = "%s/__static_library" % name,
        edition = "2018",
        srcs = ["lib.rs"],
        tags = ["manual", "nobuild"],
    )

    with_extra_toolchain(
        name = "%s/_static_library" % name,
        extra_toolchain = ":%s/test_cc_toolchain" % name,
        target = "%s/__static_library" % name,
        tags = ["manual"],
    )

    inputs_analysis_test(
        name = "%s/static_library" % name,
        target_under_test = "%s/_static_library" % name,
        expected_inputs = ["dummy.a"],
    )
