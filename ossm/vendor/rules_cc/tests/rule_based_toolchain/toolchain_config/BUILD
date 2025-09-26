load("@rules_testing//lib:util.bzl", "util")
load("//cc/toolchains:args.bzl", "cc_args")
load("//cc/toolchains:feature.bzl", "cc_feature")
load("//cc/toolchains:feature_set.bzl", "cc_feature_set")
load("//cc/toolchains:tool.bzl", "cc_tool")
load("//cc/toolchains:tool_map.bzl", "cc_tool_map")
load("//cc/toolchains/args:sysroot.bzl", "cc_sysroot")
load("//cc/toolchains/impl:external_feature.bzl", "cc_external_feature")
load("//cc/toolchains/impl:toolchain_config.bzl", "cc_legacy_file_group", "cc_toolchain_config")
load("//tests/rule_based_toolchain:analysis_test_suite.bzl", "analysis_test_suite")
load(":toolchain_config_test.bzl", "TARGETS", "TESTS")

util.helper_target(
    cc_feature_set,
    name = "all_simple_features",
    all_of = [
        ":simple_feature",
        "simple_feature2",
    ],
)

util.helper_target(
    cc_external_feature,
    name = "builtin_feature",
    feature_name = "builtin_feature",
    overridable = True,
)

util.helper_target(
    cc_args,
    name = "c_compile_args",
    actions = ["//tests/rule_based_toolchain/actions:c_compile"],
    allowlist_include_directories = ["//tests/rule_based_toolchain/testdata:subdirectory_1"],
    args = ["c_compile_args"],
    data = ["//tests/rule_based_toolchain/testdata:file1"],
)

util.helper_target(
    cc_args,
    name = "cpp_compile_args",
    actions = ["//tests/rule_based_toolchain/actions:cpp_compile"],
    args = ["cpp_compile_args"],
    env = {"CPP_COMPILE": "1"},
)

cc_tool(
    name = "c_compile_tool",
    src = "//tests/rule_based_toolchain/testdata:bin_wrapper",
    allowlist_include_directories = ["//tests/rule_based_toolchain/testdata:subdirectory_3"],
    capabilities = ["//cc/toolchains/capabilities:supports_pic"],
)

cc_sysroot(
    name = "sysroot",
    actions = [
        "//cc/toolchains/actions:cpp_compile_actions",
        "//cc/toolchains/actions:c_compile",
        "//cc/toolchains/actions:link_actions",
        "//tests/rule_based_toolchain/actions:c_compile",
        "//tests/rule_based_toolchain/actions:cpp_compile",
    ],
    sysroot = "//tests/rule_based_toolchain/testdata:directory",
)

util.helper_target(
    cc_toolchain_config,
    name = "collects_files_toolchain_config",
    args = [
        ":sysroot",
        ":c_compile_args",
    ],
    enabled_features = [":simple_feature"],
    known_features = [":compile_feature"],
    tool_map = ":compile_tool_map",
)

util.helper_target(
    cc_legacy_file_group,
    name = "collects_files_c_compile",
    actions = ["//tests/rule_based_toolchain/actions:c_compile"],
    config = ":collects_files_toolchain_config",
)

util.helper_target(
    cc_legacy_file_group,
    name = "collects_files_cpp_compile",
    actions = ["//tests/rule_based_toolchain/actions:cpp_compile"],
    config = ":collects_files_toolchain_config",
)

util.helper_target(
    cc_args,
    name = "compile_args",
    actions = ["//tests/rule_based_toolchain/actions:all_compile"],
    allowlist_include_directories = ["//tests/rule_based_toolchain/testdata:subdirectory_2"],
    args = ["compile_args"],
    data = ["//tests/rule_based_toolchain/testdata:file2"],
)

util.helper_target(
    cc_tool_map,
    name = "compile_tool_map",
    tools = {
        "//tests/rule_based_toolchain/actions:c_compile": ":c_compile_tool",
        "//tests/rule_based_toolchain/actions:cpp_compile": "//tests/rule_based_toolchain/tool:wrapped_tool",
    },
)

util.helper_target(
    cc_feature,
    name = "compile_feature",
    args = [":compile_args"],
    feature_name = "compile_feature",
)

util.helper_target(
    cc_tool_map,
    name = "c_compile_tool_map",
    tools = {
        "//tests/rule_based_toolchain/actions:c_compile": "//tests/rule_based_toolchain/tool:wrapped_tool",
    },
)

util.helper_target(
    cc_tool_map,
    name = "empty_tool_map",
    tools = {},
)

util.helper_target(
    cc_feature,
    name = "implies_simple_feature",
    args = [":c_compile_args"],
    feature_name = "implies",
    implies = [":simple_feature"],
)

util.helper_target(
    cc_feature,
    name = "overrides_feature",
    args = [":c_compile_args"],
    overrides = ":builtin_feature",
)

util.helper_target(
    cc_args,
    name = "requires_all_simple_args",
    actions = ["//tests/rule_based_toolchain/actions:c_compile"],
    args = ["--foo"],
    requires_any_of = [":all_simple_features"],
)

util.helper_target(
    cc_feature,
    name = "requires_all_simple_feature",
    args = [":c_compile_args"],
    feature_name = "requires_any_simple",
    requires_any_of = [":all_simple_features"],
)

util.helper_target(
    cc_feature,
    name = "requires_any_simple_feature",
    args = [":c_compile_args"],
    feature_name = "requires_any_simple",
    requires_any_of = [
        ":simple_feature",
        ":simple_feature2",
    ],
)

util.helper_target(
    cc_feature,
    name = "same_feature_name",
    args = [":c_compile_args"],
    feature_name = "simple_feature",
    visibility = ["//tests/rule_based_toolchain:__subpackages__"],
)

util.helper_target(
    cc_feature,
    name = "simple_feature",
    args = [":c_compile_args"],
    feature_name = "simple_feature",
)

util.helper_target(
    cc_feature,
    name = "simple_feature2",
    args = [":c_compile_args"],
    feature_name = "simple_feature2",
    visibility = ["//tests/rule_based_toolchain:__subpackages__"],
)

analysis_test_suite(
    name = "test_suite",
    targets = TARGETS,
    tests = TESTS,
)
