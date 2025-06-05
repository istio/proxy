load("@rules_testing//lib:util.bzl", "util")
load("//cc/toolchains:args.bzl", "cc_args")
load("//tests/rule_based_toolchain:analysis_test_suite.bzl", "analysis_test_suite")
load(":args_test.bzl", "TARGETS", "TESTS")

util.helper_target(
    cc_args,
    name = "simple",
    actions = ["//tests/rule_based_toolchain/actions:all_compile"],
    args = [
        "--foo",
        "foo",
    ],
    data = [
        "//tests/rule_based_toolchain/testdata:file1",
        "//tests/rule_based_toolchain/testdata:multiple",
    ],
    env = {"BAR": "bar"},
)

util.helper_target(
    cc_args,
    name = "env_only",
    actions = ["//tests/rule_based_toolchain/actions:all_compile"],
    data = [
        "//tests/rule_based_toolchain/testdata:file1",
        "//tests/rule_based_toolchain/testdata:multiple",
    ],
    env = {"BAR": "bar"},
)

util.helper_target(
    cc_args,
    name = "iterate_over_optional",
    actions = ["//cc/toolchains/actions:compile_actions"],
    args = ["{user_compile_flags}"],
    format = {"user_compile_flags": "//cc/toolchains/variables:user_compile_flags"},
    iterate_over = "//cc/toolchains/variables:user_compile_flags",
    requires_not_none = "//cc/toolchains/variables:user_compile_flags",
)

util.helper_target(
    cc_args,
    name = "with_dir",
    actions = ["//tests/rule_based_toolchain/actions:all_compile"],
    allowlist_include_directories = ["//tests/rule_based_toolchain/testdata:directory"],
    args = ["--secret-builtin-include-dir"],
)

analysis_test_suite(
    name = "test_suite",
    targets = TARGETS,
    tests = TESTS,
)
