load("@rules_testing//lib:util.bzl", "util")
load("//cc/toolchains:args.bzl", "cc_args")
load("//cc/toolchains:feature.bzl", "cc_feature")
load("//cc/toolchains:feature_constraint.bzl", "cc_feature_constraint")
load("//cc/toolchains:feature_set.bzl", "cc_feature_set")
load("//cc/toolchains:mutually_exclusive_category.bzl", "cc_mutually_exclusive_category")
load("//cc/toolchains/impl:external_feature.bzl", "cc_external_feature")
load("//tests/rule_based_toolchain:analysis_test_suite.bzl", "analysis_test_suite")
load(":features_test.bzl", "TARGETS", "TESTS")

util.helper_target(
    cc_args,
    name = "c_compile_args",
    actions = ["//tests/rule_based_toolchain/actions:c_compile"],
    args = ["c"],
    data = ["//tests/rule_based_toolchain/testdata:file1"],
)

util.helper_target(
    cc_feature,
    name = "simple",
    args = [":c_compile_args"],
    feature_name = "feature_name",
    visibility = ["//tests/rule_based_toolchain:__subpackages__"],
)

util.helper_target(
    cc_feature,
    name = "simple2",
    args = [":c_compile_args"],
    feature_name = "simple2",
)

util.helper_target(
    cc_feature_set,
    name = "feature_set",
    all_of = [
        ":simple",
        ":simple2",
    ],
)

util.helper_target(
    cc_feature,
    name = "requires",
    args = [":c_compile_args"],
    feature_name = "requires",
    requires_any_of = [":feature_set"],
)

util.helper_target(
    cc_feature,
    name = "implies",
    args = [":c_compile_args"],
    feature_name = "implies",
    implies = [":simple"],
)

cc_mutually_exclusive_category(
    name = "category",
)

util.helper_target(
    cc_feature,
    name = "mutual_exclusion_feature",
    args = [":c_compile_args"],
    feature_name = "mutual_exclusion",
    mutually_exclusive = [
        ":simple",
        ":category",
    ],
)

util.helper_target(
    cc_feature_constraint,
    name = "direct_constraint",
    all_of = [":simple"],
    none_of = [":simple2"],
    visibility = ["//tests/rule_based_toolchain:__subpackages__"],
)

util.helper_target(
    cc_feature_constraint,
    name = "transitive_constraint",
    all_of = [
        ":direct_constraint",
        ":requires",
    ],
    none_of = [":implies"],
)

util.helper_target(
    cc_external_feature,
    name = "builtin_feature",
    feature_name = "builtin_feature",
    overridable = True,
)

util.helper_target(
    cc_feature,
    name = "overrides",
    args = [":c_compile_args"],
    overrides = ":builtin_feature",
)

util.helper_target(
    cc_feature,
    name = "sentinel_feature",
    feature_name = "sentinel_feature_name",
)

util.helper_target(
    cc_args,
    name = "args_with_dir",
    actions = ["//tests/rule_based_toolchain/actions:c_compile"],
    allowlist_include_directories = ["//tests/rule_based_toolchain/testdata:subdirectory_1"],
    args = ["--include-builtin-dirs"],
)

util.helper_target(
    cc_feature,
    name = "feature_with_dir",
    args = [":args_with_dir"],
    feature_name = "feature_with_dir",
)

analysis_test_suite(
    name = "test_suite",
    targets = TARGETS,
    tests = TESTS,
)
