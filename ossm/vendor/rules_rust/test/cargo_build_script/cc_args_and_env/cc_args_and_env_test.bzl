"""
Tests for the cargo_build_script processing of underlying cc args and env variables.

We need control over the underlying cc_toolchain args. This is provided by
cargo_build_script_with_extra_cc_compile_flags().

To verify the processed cargo cc_args, we use cc_args_and_env_analysis_test().
"""

load("@bazel_skylib//lib:unittest.bzl", "analysistest", "asserts")
load("@rules_cc//cc:action_names.bzl", "ACTION_NAMES", "ACTION_NAME_GROUPS")
load("@rules_cc//cc:cc_toolchain_config_lib.bzl", "action_config", "feature", "flag_group", "flag_set", "tool", "tool_path")
load("@rules_cc//cc:defs.bzl", "cc_toolchain")
load("@rules_cc//cc/common:cc_common.bzl", "cc_common")
load("@rules_cc//cc/toolchains:cc_toolchain_config_info.bzl", "CcToolchainConfigInfo")
load("//cargo:defs.bzl", "cargo_build_script")

_EXPECTED_CC_TOOLCHAIN_TOOLS = {
    "AR": "/usr/fake/ar",
    "CC": "/usr/fake/gcc",
    "CXX": "/usr/fake/g++",
}

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
        feature(
            name = "default_cpp_flags",
            enabled = True,
            flag_sets = [
                flag_set(
                    actions = ACTION_NAME_GROUPS.all_cpp_compile_actions,
                    flag_groups = ([
                        flag_group(
                            flags = ctx.attr.extra_cxx_compile_flags,
                        ),
                    ]),
                ),
            ],
        ),
        feature(
            name = "default_ar_flags",
            enabled = True,
            flag_sets = [
                flag_set(
                    actions = [ACTION_NAMES.cpp_link_static_library],
                    flag_groups = ([
                        flag_group(
                            # Simulate a case where a toolchain always passes
                            # rcsD to `ar`.
                            flags = ["rcsD"],
                        ),
                    ]),
                ),
                flag_set(
                    actions = [ACTION_NAMES.cpp_link_static_library],
                    flag_groups = ([
                        flag_group(
                            flags = ctx.attr.extra_ar_flags,
                        ),
                    ]),
                ),
            ],
        ),
    ]

    tool_paths = []
    action_configs = []
    if ctx.attr.legacy_cc_toolchain:
        tool_paths = [
            tool_path(
                name = "gcc",
                path = _EXPECTED_CC_TOOLCHAIN_TOOLS["CC"],
            ),
            tool_path(
                name = "cpp",
                path = _EXPECTED_CC_TOOLCHAIN_TOOLS["CXX"],
            ),
            tool_path(
                name = "ar",
                path = _EXPECTED_CC_TOOLCHAIN_TOOLS["AR"],
            ),
            # These need to be set to something to create a toolchain, but
            # is not tested here.
            tool_path(
                name = "ld",
                path = "/usr/ignored/false",
            ),
            tool_path(
                name = "nm",
                path = "/usr/ignored/false",
            ),
            tool_path(
                name = "objdump",
                path = "/usr/ignored/false",
            ),
            tool_path(
                name = "strip",
                path = "/usr/ignored/false",
            ),
        ]
    else:
        action_configs = (
            action_config(
                action_name = ACTION_NAMES.c_compile,
                tools = [
                    tool(path = _EXPECTED_CC_TOOLCHAIN_TOOLS["CC"]),
                ],
            ),
            action_config(
                action_name = ACTION_NAMES.cpp_compile,
                tools = [
                    tool(path = _EXPECTED_CC_TOOLCHAIN_TOOLS["CXX"]),
                ],
            ),
            action_config(
                action_name = ACTION_NAMES.cpp_link_static_library,
                tools = [
                    tool(path = _EXPECTED_CC_TOOLCHAIN_TOOLS["AR"]),
                ],
            ),
        )

    config_info = cc_common.create_cc_toolchain_config_info(
        ctx = ctx,
        action_configs = action_configs,
        toolchain_identifier = "test-cc-toolchain",
        host_system_name = "unknown",
        target_system_name = "unknown",
        target_cpu = "unknown",
        target_libc = "unknown",
        compiler = "unknown",
        abi_version = "unknown",
        abi_libc_version = "unknown",
        features = features,
        tool_paths = tool_paths,
    )
    return config_info

test_cc_config = rule(
    implementation = _test_cc_config_impl,
    attrs = {
        "extra_ar_flags": attr.string_list(),
        "extra_cc_compile_flags": attr.string_list(),
        "extra_cxx_compile_flags": attr.string_list(),
        "legacy_cc_toolchain": attr.bool(default = False),
    },
    provides = [CcToolchainConfigInfo],
)

def _with_extra_toolchains_transition_impl(_settings, attr):
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

    for env_var, expected_path in _EXPECTED_CC_TOOLCHAIN_TOOLS.items():
        if ctx.attr.legacy_cc_toolchain and env_var == "CXX":
            # When using the legacy tool_path toolchain configuration approach,
            # the CXX tool is forced to be the same as the the CC tool.
            # See: https://github.com/bazelbuild/bazel/blob/14840856986f21b54330e352b83d687825648889/src/main/starlark/builtins_bzl/common/cc/toolchain_config/legacy_features.bzl#L1296-L1304
            expected_path = _EXPECTED_CC_TOOLCHAIN_TOOLS["CC"]

        actual_path = cargo_action.env.get(env_var)
        asserts.false(
            env,
            actual_path == None,
            "error: '{}' tool unset".format(env_var),
        )
        asserts.equals(
            env,
            expected_path,
            actual_path,
            "error: '{}' tool path '{}' does not match expected '{}'".format(
                env_var,
                actual_path,
                expected_path,
            ),
        )

    _ENV_VAR_TO_EXPECTED_ARGS = {
        "CFLAGS": ctx.attr.expected_cflags,
        "CXXFLAGS": ctx.attr.expected_cxxflags,
    }

    for env_var, expected_flags in _ENV_VAR_TO_EXPECTED_ARGS.items():
        actual_flags = cargo_action.env[env_var].split(" ")
        for flag in expected_flags:
            asserts.true(
                env,
                flag in actual_flags,
                "error: expected '{}' to be in cargo {}: '{}'".format(flag, env_var, actual_flags),
            )

    arflags = cargo_action.env["ARFLAGS"]
    asserts.equals(
        env,
        [],
        arflags.split(" ") if arflags else [],
        "ARFLAGS is intentionally always empty. cc-rs tightly controls the " +
        "archiver flags in such a way that forwarding standard flags as " +
        "set up by most Bazel C/C++ toolchains is extremely error-prone.",
    )

    return analysistest.end(env)

cc_args_and_env_analysis_test = analysistest.make(
    impl = _cc_args_and_env_analysis_test_impl,
    doc = """An analysistest to examine the custom cargo flags of an cargo_build_script target.""",
    attrs = {
        "expected_cflags": attr.string_list(default = ["-Wall"]),
        "expected_cxxflags": attr.string_list(default = ["-fno-rtti"]),
        "legacy_cc_toolchain": attr.bool(default = False),
    },
)

def cargo_build_script_with_extra_cc_compile_flags(
        *,
        name,
        extra_cc_compile_flags = ["-Wall"],
        extra_cxx_compile_flags = ["-fno-rtti"],
        extra_ar_flags = ["-x"],
        legacy_cc_toolchain = False):
    """Produces a test cargo_build_script target that's set up to use a custom cc_toolchain with the extra_cc_compile_flags.

    This is achieved by creating a cascade of targets:
    1. We generate a cc_toolchain target configured with the extra_cc_compile_flags.
    2. We use a custom rule to transition to a build configuration that uses
    this cc_toolchain and use a custom provider to collect the target actions,
    which contain the cargo_build_script action with the processed cc args.

    Args:
      name: The name of the test target.
      extra_cc_compile_flags: Extra C/C++ args for the cc_toolchain.
      extra_cxx_compile_flags: Extra C++-specific args for the cc_toolchain.
      extra_ar_flags: Extra archiver args for the cc_toolchain.
      legacy_cc_toolchain: Enables legacy tool_path configuration of the cc
        cc toolchain.
    """

    test_cc_config(
        name = "%s/cc_toolchain_config" % name,
        extra_cc_compile_flags = extra_cc_compile_flags,
        extra_cxx_compile_flags = extra_cxx_compile_flags,
        extra_ar_flags = extra_ar_flags,
        legacy_cc_toolchain = legacy_cc_toolchain,
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

def bindir_relative_test(name):
    cargo_build_script_with_extra_cc_compile_flags(
        name = "%s/cargo_build_script" % name,
        extra_cc_compile_flags = ["-B", "test/relative/path"],
    )
    cc_args_and_env_analysis_test(
        name = name,
        target_under_test = "%s/cargo_build_script" % name,
        expected_cflags = ["-B", "${pwd}/test/relative/path"],
    )

def bindir_absolute_test(name):
    cargo_build_script_with_extra_cc_compile_flags(
        name = "%s/cargo_build_script" % name,
        extra_cc_compile_flags = ["-B", "/test/absolute/path"],
    )
    cc_args_and_env_analysis_test(
        name = name,
        target_under_test = "%s/cargo_build_script" % name,
        expected_cflags = ["-B", "/test/absolute/path"],
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

def legacy_cc_toolchain_test(name):
    cargo_build_script_with_extra_cc_compile_flags(
        name = "%s/cargo_build_script" % name,
        legacy_cc_toolchain = True,
    )
    cc_args_and_env_analysis_test(
        name = name,
        target_under_test = "%s/cargo_build_script" % name,
        legacy_cc_toolchain = True,
    )
