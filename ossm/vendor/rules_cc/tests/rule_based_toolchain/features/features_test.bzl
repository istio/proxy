# Copyright 2024 The Bazel Authors. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""Tests for features for the rule based toolchain."""

load(
    "//cc:cc_toolchain_config_lib.bzl",
    legacy_feature_set = "feature_set",
    legacy_flag_group = "flag_group",
    legacy_flag_set = "flag_set",
)
load(
    "//cc/toolchains:cc_toolchain_info.bzl",
    "ActionTypeInfo",
    "ArgsInfo",
    "FeatureConstraintInfo",
    "FeatureInfo",
    "FeatureSetInfo",
    "MutuallyExclusiveCategoryInfo",
)
load(
    "//cc/toolchains/impl:legacy_converter.bzl",
    "convert_feature",
    "convert_feature_constraint",
)

visibility("private")

_C_COMPILE_FILE = "tests/rule_based_toolchain/testdata/file1"
_SUBDIR1 = "tests/rule_based_toolchain/testdata/subdir1"
_SUBDIR1_FILES = ["tests/rule_based_toolchain/testdata/subdir1/file_foo"]

def _sentinel_feature_test(env, targets):
    sentinel_feature = env.expect.that_target(targets.sentinel_feature).provider(FeatureInfo)
    sentinel_feature.name().equals("sentinel_feature_name")
    sentinel_feature.args().args().contains_exactly([])

def _simple_feature_test(env, targets):
    simple = env.expect.that_target(targets.simple).provider(FeatureInfo)
    simple.name().equals("feature_name")
    simple.args().args().contains_exactly([targets.c_compile_args.label])
    simple.enabled().equals(False)
    simple.overrides().is_none()
    simple.overridable().equals(False)

    simple.args().files().contains_exactly([_C_COMPILE_FILE])
    c_compile_action = simple.args().by_action().get(
        targets.c_compile_args[ArgsInfo].actions.to_list()[0],
    )
    c_compile_action.files().contains_exactly([_C_COMPILE_FILE])
    c_compile_action.args().contains_exactly([targets.c_compile_args[ArgsInfo]])

    legacy = convert_feature(simple.actual)
    env.expect.that_str(legacy.name).equals("feature_name")
    env.expect.that_bool(legacy.enabled).equals(False)
    env.expect.that_collection(legacy.flag_sets).contains_exactly([
        legacy_flag_set(
            actions = ["c_compile"],
            with_features = [],
            flag_groups = [legacy_flag_group(flags = ["c"])],
        ),
    ])

def _feature_collects_requirements_test(env, targets):
    ft = env.expect.that_target(targets.requires).provider(FeatureInfo)
    ft.requires_any_of().contains_exactly([
        targets.feature_set.label,
    ])

    legacy = convert_feature(ft.actual)
    env.expect.that_collection(legacy.requires).contains_exactly([
        legacy_feature_set(features = ["feature_name", "simple2"]),
    ])

def _feature_collects_implies_test(env, targets):
    env.expect.that_target(targets.implies).provider(
        FeatureInfo,
    ).implies().contains_exactly([
        targets.simple.label,
    ])

def _feature_collects_mutual_exclusion_test(env, targets):
    env.expect.that_target(targets.simple).provider(
        MutuallyExclusiveCategoryInfo,
    ).name().equals("feature_name")
    env.expect.that_target(targets.mutual_exclusion_feature).provider(
        FeatureInfo,
    ).mutually_exclusive().contains_exactly([
        targets.simple.label,
        targets.category.label,
    ])

def _feature_set_collects_features_test(env, targets):
    env.expect.that_target(targets.feature_set).provider(
        FeatureSetInfo,
    ).features().contains_exactly([
        targets.simple.label,
        targets.simple2.label,
    ])

def _feature_constraint_collects_direct_features_test(env, targets):
    constraint = env.expect.that_target(targets.direct_constraint).provider(
        FeatureConstraintInfo,
    )
    constraint.all_of().contains_exactly([targets.simple.label])
    constraint.none_of().contains_exactly([targets.simple2.label])

def _feature_constraint_collects_transitive_features_test(env, targets):
    constraint = env.expect.that_target(targets.transitive_constraint).provider(
        FeatureConstraintInfo,
    )
    constraint.all_of().contains_exactly([
        targets.simple.label,
        targets.requires.label,
    ])
    constraint.none_of().contains_exactly([
        targets.simple2.label,
        targets.implies.label,
    ])

    legacy = convert_feature_constraint(constraint.actual)
    env.expect.that_collection(legacy.features).contains_exactly([
        "feature_name",
        "requires",
    ])
    env.expect.that_collection(legacy.not_features).contains_exactly([
        "simple2",
        "implies",
    ])

def _external_feature_is_a_feature_test(env, targets):
    external_feature = env.expect.that_target(targets.builtin_feature).provider(
        FeatureInfo,
    )
    external_feature.name().equals("builtin_feature")

    # It's not a string, but we don't have a factory for the type.
    env.expect.that_str(convert_feature(external_feature.actual)).equals(None)

def _feature_can_be_overridden_test(env, targets):
    overrides = env.expect.that_target(targets.overrides).provider(FeatureInfo)
    overrides.name().equals("builtin_feature")
    overrides.overrides().some().label().equals(targets.builtin_feature.label)

def _feature_with_directory_test(env, targets):
    with_dir = env.expect.that_target(targets.feature_with_dir).provider(FeatureInfo)
    with_dir.allowlist_include_directories().contains_exactly([_SUBDIR1])

    c_compile = env.expect.that_target(targets.feature_with_dir).provider(FeatureInfo).args().by_action().get(
        targets.c_compile[ActionTypeInfo],
    )
    c_compile.files().contains_at_least(_SUBDIR1_FILES)

TARGETS = [
    ":args_with_dir",
    ":builtin_feature",
    ":c_compile_args",
    ":category",
    ":direct_constraint",
    ":feature_set",
    ":feature_with_dir",
    ":implies",
    ":mutual_exclusion_feature",
    ":overrides",
    ":requires",
    ":sentinel_feature",
    ":simple",
    ":simple2",
    ":transitive_constraint",
    "//tests/rule_based_toolchain/actions:c_compile",
]

# @unsorted-dict-items
TESTS = {
    "sentinel_feature_test": _sentinel_feature_test,
    "simple_feature_test": _simple_feature_test,
    "feature_collects_requirements_test": _feature_collects_requirements_test,
    "feature_collects_implies_test": _feature_collects_implies_test,
    "feature_collects_mutual_exclusion_test": _feature_collects_mutual_exclusion_test,
    "feature_set_collects_features_test": _feature_set_collects_features_test,
    "feature_constraint_collects_direct_features_test": _feature_constraint_collects_direct_features_test,
    "feature_constraint_collects_transitive_features_test": _feature_constraint_collects_transitive_features_test,
    "external_feature_is_a_feature_test": _external_feature_is_a_feature_test,
    "feature_can_be_overridden_test": _feature_can_be_overridden_test,
    "feature_with_directory_test": _feature_with_directory_test,
}
