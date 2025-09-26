load("@rules_testing//lib:util.bzl", "util")
load("//cc/toolchains:actions.bzl", "cc_action_type", "cc_action_type_set")
load("//tests/rule_based_toolchain:analysis_test_suite.bzl", "analysis_test_suite")
load(":actions_test.bzl", "TARGETS", "TESTS")

util.helper_target(
    cc_action_type,
    name = "c_compile",
    action_name = "c_compile",
    visibility = ["//tests/rule_based_toolchain:__subpackages__"],
)

util.helper_target(
    cc_action_type,
    name = "cpp_compile",
    action_name = "cpp_compile",
    visibility = ["//tests/rule_based_toolchain:__subpackages__"],
)

util.helper_target(
    cc_action_type_set,
    name = "all_compile",
    actions = [
        ":c_compile",
        ":cpp_compile",
    ],
    visibility = ["//tests/rule_based_toolchain:__subpackages__"],
)

analysis_test_suite(
    name = "test_suite",
    targets = TARGETS,
    tests = TESTS,
)
