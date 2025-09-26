"""
Tests for the cargo_build_script processing of underlying cc args and env variables.

We need control over the underlying cc_toolchain args. This is provided by
cargo_build_script_with_extra_cc_compile_flags().

To verify the processed cargo cc_args, we use cc_args_and_env_analysis_test().
"""

load("@bazel_skylib//lib:unittest.bzl", "analysistest", "asserts")
load("@rules_cc//cc:action_names.bzl", "ACTION_NAME_GROUPS")
load("@rules_cc//cc:cc_toolchain_config_lib.bzl", "feature", "flag_group", "flag_set")
load("@rules_cc//cc:defs.bzl", "cc_toolchain")
load("//cargo:defs.bzl", "cargo_build_script")

def _test_cc_config_impl(ctx):
    features = [
        feature(
            name = "default_compiler_flags",
            enabled = True,
            flag_sets = [
                flag_set(
                    actions = ACTION_NAME_GROUPS.all_cc_compile_actions,
                    flag_groups = ([
                        flag_group(
                            flags = ctx.attr.extra_cc_compile_flags,
                        ),
                    ]),
                ),
            ],
        ),
    ]
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
        features = features,
    )
    return config_info

test_cc_config = rule(
    implementation = _test_cc_config_impl,
    attrs = {
        "extra_cc_compile_flags": attr.string_list(),
    },
    provides = [CcToolchainConfigInfo],
)

def _with_extra_toolchains_transition_impl(_setings, attr):
    return {"//command_line_option:extra_toolchains": attr.extra_toolchains}

with_extra_toolchains_transition = transition(
    implementation = _with_extra_toolchains_transition_impl,
    inputs = [],
    outputs = ["//command_line_option:extra_toolchains"],
)

DepActionsInfo = provider(
    "Contains information about dependencies actions.",
    fields = {"actions": "List[Action]"},
)

def _with_extra_toolchains_impl(ctx):
    return [
        DepActionsInfo(actions = ctx.attr.target[0].actions),
    ]

with_extra_toolchains = rule(
    implementation = _with_extra_toolchains_impl,
    attrs = {
        "extra_toolchains": attr.string_list(),
        "target": attr.label(cfg = with_extra_toolchains_transition),
    },
)

def _cc_args_and_env_analysis_test_impl(ctx):
    env = analysistest.begin(ctx)
    tut = analysistest.target_under_test(env)
    cargo_action = tut[DepActionsInfo].actions[0]
    cflags = cargo_action.env["CFLAGS"].split(" ")
    for flag in ctx.attr.expected_cflags:
        asserts.true(
            env,
            flag in cflags,
            "error: expected '{}' to be in cargo CFLAGS: '{}'".format(flag, cflags),
        )

    return analysistest.end(env)

cc_args_and_env_analysis_test = analysistest.make(
    impl = _cc_args_and_env_analysis_test_impl,
    doc = """An analysistest to examine the custom cargo flags of an cargo_build_script target.""",
    attrs = {
        "expected_cflags": attr.string_list(),
    },
)

def cargo_build_script_with_extra_cc_compile_flags(
        name,
        extra_cc_compile_flags):
    """Produces a test cargo_build_script target that's set up to use a custom cc_toolchain with the extra_cc_compile_flags.

    This is achieved by creating a cascade of targets:
    1. We generate a cc_toolchain target configured with the extra_cc_compile_flags.
    2. We use a custom rule to transition to a build configuration that uses
    this cc_toolchain and use a custom provider to collect the target actions,
    which contain the cargo_build_script action with the processed cc args.

    Args:
      name: The name of the test target.
      extra_cc_compile_flags: Extra args for the cc_toolchain.
    """

    test_cc_config(
        name = "%s/cc_toolchain_config" % name,
        extra_cc_compile_flags = extra_cc_compile_flags,
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
    )
    native.toolchain(
        name = "%s/test_cc_toolchain" % name,
        toolchain = ":%s/test_cc_toolchain_impl" % name,
        toolchain_type = "@bazel_tools//tools/cpp:toolchain_type",
    )
    cargo_build_script(
        name = "%s/cargo_build_script_impl" % name,
        edition = "2018",
        srcs = ["build.rs"],
        tags = ["manual", "nobuild"],
    )
    with_extra_toolchains(
        name = name,
        extra_toolchains = ["//test/cargo_build_script/cc_args_and_env:%s/test_cc_toolchain" % name],
        target = "%s/cargo_build_script_impl" % name,
        tags = ["manual"],
    )

def sysroot_relative_test(name):
    cargo_build_script_with_extra_cc_compile_flags(
        name = "%s/cargo_build_script" % name,
        extra_cc_compile_flags = ["--sysroot=test/relative/sysroot"],
    )
    cc_args_and_env_analysis_test(
        name = name,
        target_under_test = "%s/cargo_build_script" % name,
        expected_cflags = ["--sysroot=${pwd}/test/relative/sysroot"],
    )

def sysroot_absolute_test(name):
    cargo_build_script_with_extra_cc_compile_flags(
        name = "%s/cargo_build_script" % name,
        extra_cc_compile_flags = ["--sysroot=/test/absolute/sysroot"],
    )
    cc_args_and_env_analysis_test(
        name = name,
        target_under_test = "%s/cargo_build_script" % name,
        expected_cflags = ["--sysroot=/test/absolute/sysroot"],
    )

def sysroot_next_absolute_test(name):
    cargo_build_script_with_extra_cc_compile_flags(
        name = "%s/cargo_build_script" % name,
        extra_cc_compile_flags = ["--sysroot=/test/absolute/sysroot", "test/relative/another"],
    )
    cc_args_and_env_analysis_test(
        name = name,
        target_under_test = "%s/cargo_build_script" % name,
        expected_cflags = ["--sysroot=/test/absolute/sysroot", "test/relative/another"],
    )

def isystem_relative_test(name):
    cargo_build_script_with_extra_cc_compile_flags(
        name = "%s/cargo_build_script" % name,
        extra_cc_compile_flags = ["-isystem", "test/relative/path"],
    )
    cc_args_and_env_analysis_test(
        name = name,
        target_under_test = "%s/cargo_build_script" % name,
        expected_cflags = ["-isystem", "${pwd}/test/relative/path"],
    )

def isystem_absolute_test(name):
    cargo_build_script_with_extra_cc_compile_flags(
        name = "%s/cargo_build_script" % name,
        extra_cc_compile_flags = ["-isystem", "/test/absolute/path"],
    )
    cc_args_and_env_analysis_test(
        name = name,
        target_under_test = "%s/cargo_build_script" % name,
        expected_cflags = ["-isystem", "/test/absolute/path"],
    )

def fsanitize_ignorelist_relative_test(name):
    cargo_build_script_with_extra_cc_compile_flags(
        name = "%s/cargo_build_script" % name,
        extra_cc_compile_flags = ["-fsanitize-ignorelist=test/relative/path"],
    )
    cc_args_and_env_analysis_test(
        name = name,
        target_under_test = "%s/cargo_build_script" % name,
        expected_cflags = ["-fsanitize-ignorelist=${pwd}/test/relative/path"],
    )

def fsanitize_ignorelist_absolute_test(name):
    cargo_build_script_with_extra_cc_compile_flags(
        name = "%s/cargo_build_script" % name,
        extra_cc_compile_flags = ["-fsanitize-ignorelist=/test/absolute/path"],
    )
    cc_args_and_env_analysis_test(
        name = name,
        target_under_test = "%s/cargo_build_script" % name,
        expected_cflags = ["-fsanitize-ignorelist=/test/absolute/path"],
    )
