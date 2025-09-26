load("//cc/toolchains:directory_tool.bzl", "cc_directory_tool")
load("//cc/toolchains:tool.bzl", "cc_tool")
load("//tests/rule_based_toolchain:analysis_test_suite.bzl", "analysis_test_suite")
load(":tool_test.bzl", "TARGETS", "TESTS")

cc_tool(
    name = "tool",
    src = "//tests/rule_based_toolchain/testdata:bin_wrapper.sh",
    capabilities = ["//cc/toolchains/capabilities:supports_pic"],
    data = ["//tests/rule_based_toolchain/testdata:bin"],
    tags = ["requires-network"],
)

cc_tool(
    name = "wrapped_tool",
    src = "//tests/rule_based_toolchain/testdata:bin_wrapper",
    visibility = ["//tests/rule_based_toolchain:__subpackages__"],
)

cc_tool(
    name = "tool_with_allowlist_include_directories",
    src = "//tests/rule_based_toolchain/testdata:bin_wrapper.sh",
    allowlist_include_directories = ["//tests/rule_based_toolchain/testdata:directory"],
    visibility = ["//tests/rule_based_toolchain:__subpackages__"],
)

cc_directory_tool(
    name = "directory_tool",
    data = ["bin"],
    directory = "//tests/rule_based_toolchain/testdata:directory",
    executable = "bin_wrapper.sh",
    tags = ["requires-network"],
)

analysis_test_suite(
    name = "test_suite",
    targets = TARGETS,
    tests = TESTS,
)
