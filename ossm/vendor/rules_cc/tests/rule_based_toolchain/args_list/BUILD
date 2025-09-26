load("@rules_testing//lib:util.bzl", "util")
load("//cc/toolchains:args.bzl", "cc_args")
load("//cc/toolchains:args_list.bzl", "cc_args_list")
load("//tests/rule_based_toolchain:analysis_test_suite.bzl", "analysis_test_suite")
load(":args_list_test.bzl", "TARGETS", "TESTS")

util.helper_target(
    cc_args,
    name = "c_compile_args",
    actions = ["//tests/rule_based_toolchain/actions:c_compile"],
    args = ["c"],
    data = ["//tests/rule_based_toolchain/testdata:file1"],
    visibility = ["//tests/rule_based_toolchain:__subpackages__"],
)

util.helper_target(
    cc_args,
    name = "cpp_compile_args",
    actions = ["//tests/rule_based_toolchain/actions:cpp_compile"],
    args = ["cpp"],
    data = ["//tests/rule_based_toolchain/testdata:file2"],
    visibility = ["//tests/rule_based_toolchain:__subpackages__"],
)

util.helper_target(
    cc_args,
    name = "all_compile_args",
    actions = ["//tests/rule_based_toolchain/actions:all_compile"],
    args = ["all"],
    data = ["//tests/rule_based_toolchain/testdata:multiple1"],
    visibility = ["//tests/rule_based_toolchain:__subpackages__"],
)

util.helper_target(
    cc_args_list,
    name = "args_list",
    args = [
        ":c_compile_args",
        ":cpp_compile_args",
        ":all_compile_args",
    ],
    visibility = ["//tests/rule_based_toolchain:__subpackages__"],
)

util.helper_target(
    cc_args,
    name = "args_with_dir_1",
    actions = ["//tests/rule_based_toolchain/actions:c_compile"],
    allowlist_include_directories = ["//tests/rule_based_toolchain/testdata:subdirectory_1"],
    args = ["dir1"],
    visibility = ["//tests/rule_based_toolchain:__subpackages__"],
)

util.helper_target(
    cc_args,
    name = "args_with_dir_2",
    actions = ["//tests/rule_based_toolchain/actions:cpp_compile"],
    allowlist_include_directories = ["//tests/rule_based_toolchain/testdata:subdirectory_2"],
    args = ["dir2"],
    visibility = ["//tests/rule_based_toolchain:__subpackages__"],
)

util.helper_target(
    cc_args_list,
    name = "args_list_with_dir",
    args = [
        ":args_with_dir_1",
        ":args_with_dir_2",
    ],
    visibility = ["//tests/rule_based_toolchain:__subpackages__"],
)

analysis_test_suite(
    name = "test_suite",
    targets = TARGETS,
    tests = TESTS,
)
