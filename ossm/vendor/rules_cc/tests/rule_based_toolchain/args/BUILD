load("@rules_testing//lib:util.bzl", "util")
load("//cc/toolchains:args.bzl", "cc_args")
load("//cc/toolchains/impl:variables.bzl", "cc_variable", "types")
load("//tests/rule_based_toolchain:analysis_test_suite.bzl", "analysis_test_suite")
load("//tests/rule_based_toolchain:testing_rules.bzl", "expect_failure_test")
load(":args_test.bzl", "TARGETS", "TESTS")

cc_variable(
    name = "some_variable",
    type = types.string,
)

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
    name = "env_only_requires",
    actions = ["//cc/toolchains/actions:compile_actions"],
    env = {"BAR": "{dependency_file}"},
    format = {"dependency_file": "//cc/toolchains/variables:dependency_file"},
    requires_not_none = "//cc/toolchains/variables:dependency_file",
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

util.helper_target(
    cc_args,
    name = "good_env_format",
    actions = ["//cc/toolchains/actions:compile_actions"],
    env = {"FOO": "{gcov_gcno_file}"},
    format = {"gcov_gcno_file": "//cc/toolchains/variables:gcov_gcno_file"},
)

util.helper_target(
    cc_args,
    name = "good_env_format_optional",
    actions = ["//cc/toolchains/actions:compile_actions"],
    env = {"FOO": "{dependency_file}"},
    format = {"dependency_file": "//cc/toolchains/variables:dependency_file"},
    requires_not_none = "//cc/toolchains/variables:dependency_file",
)

util.helper_target(
    cc_args,
    name = "bad_env_format_list",
    actions = ["//cc/toolchains/actions:compile_actions"],
    env = {"FOO": "{preprocessor_defines}"},
    format = {"preprocessor_defines": "//cc/toolchains/variables:preprocessor_defines"},
    tags = ["manual"],
)

expect_failure_test(
    name = "bad_env_format_list_test",
    failure_message = "format only works on string, file, or directory type variables, but preprocessor_defines has type List[string]",
    target = ":bad_env_format_list",
)

util.helper_target(
    cc_args,
    name = "bad_env_format_optional",
    actions = ["//cc/toolchains/actions:compile_actions"],
    env = {"FOO": "{user_compile_flags}"},
    format = {"user_compile_flags": "//cc/toolchains/variables:user_compile_flags"},
    tags = ["manual"],
)

expect_failure_test(
    name = "bad_env_format_optional_test",
    failure_message = "format only works on string, file, or directory type variables, but user_compile_flags has type List[string]",
    target = ":bad_env_format_optional",
)

util.helper_target(
    cc_args,
    name = "bad_env_format_wrong_action",
    actions = ["//cc/toolchains/actions:compile_actions"],
    env = {"FOO": "{runtime_solib_name}"},
    format = {"runtime_solib_name": "//cc/toolchains/variables:runtime_solib_name"},
    tags = ["manual"],
)

expect_failure_test(
    name = "bad_env_format_wrong_action_test",
    failure_message = "runtime_solib_name is inaccessible from the action",
    target = ":bad_env_format_wrong_action",
)

analysis_test_suite(
    name = "test_suite",
    targets = TARGETS,
    tests = TESTS,
)
