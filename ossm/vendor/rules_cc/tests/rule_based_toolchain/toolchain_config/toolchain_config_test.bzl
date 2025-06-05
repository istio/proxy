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
"""Tests for the cc_toolchain_config rule."""

load(
    "//cc:cc_toolchain_config_lib.bzl",
    legacy_action_config = "action_config",
    legacy_feature = "feature",
    legacy_flag_group = "flag_group",
    legacy_flag_set = "flag_set",
    legacy_tool = "tool",
)
load("//cc/toolchains:cc_toolchain_info.bzl", "ActionTypeInfo", "ToolchainConfigInfo")
load("//cc/toolchains/impl:legacy_converter.bzl", "convert_toolchain")
load("//cc/toolchains/impl:toolchain_config_info.bzl", _toolchain_config_info = "toolchain_config_info")
load("//tests/rule_based_toolchain:subjects.bzl", "result_fn_wrapper", "subjects")

visibility("private")

toolchain_config_info = result_fn_wrapper(_toolchain_config_info)

_COLLECTED_CPP_COMPILE_FILES = [
    # From :compile_config's tool
    "tests/rule_based_toolchain/testdata/bin",
    "tests/rule_based_toolchain/testdata/bin_wrapper",
    # From :compile_feature's args
    "tests/rule_based_toolchain/testdata/file2",
    # From :compile_feature's args' allowlist_include_directories
    "tests/rule_based_toolchain/testdata/subdir2/file_bar",
]

_COLLECTED_C_COMPILE_FILES = _COLLECTED_CPP_COMPILE_FILES + [
    # From :c_compile_args
    "tests/rule_based_toolchain/testdata/file1",
    # From :c_compile_args's allowlist_include_directories
    "tests/rule_based_toolchain/testdata/subdir1/file_foo",
    # From :c_compile_tool's allowlist_include_directories
    "tests/rule_based_toolchain/testdata/subdir3/file_baz",
]

def _expect_that_toolchain(env, expr = None, **kwargs):
    return env.expect.that_value(
        value = toolchain_config_info(label = Label("//:toolchain"), **kwargs),
        expr = expr,
        factory = subjects.result(subjects.ToolchainConfigInfo),
    )

def _missing_tool_map_invalid_test(env, _targets):
    _expect_that_toolchain(
        env,
        tool_map = None,
        expr = "missing_tool_map",
    ).err().contains(
        "tool_map is required",
    )

def _empty_toolchain_valid_test(env, targets):
    _expect_that_toolchain(
        env,
        tool_map = targets.empty_tool_map,  # tool_map is always required.
    ).ok()

def _duplicate_feature_names_invalid_test(env, targets):
    _expect_that_toolchain(
        env,
        known_features = [targets.simple_feature, targets.same_feature_name],
        tool_map = targets.empty_tool_map,
        expr = "duplicate_feature_name",
    ).err().contains_all_of([
        "The feature name simple_feature was defined by",
        targets.same_feature_name.label,
        targets.simple_feature.label,
    ])

    # Overriding a feature gives it the same name. Ensure this isn't blocked.
    _expect_that_toolchain(
        env,
        known_features = [targets.builtin_feature, targets.overrides_feature],
        tool_map = targets.empty_tool_map,
        expr = "override_feature",
    ).ok()

def _feature_config_implies_missing_feature_invalid_test(env, targets):
    _expect_that_toolchain(
        env,
        expr = "feature_with_implies",
        known_features = [targets.simple_feature, targets.implies_simple_feature],
        tool_map = targets.empty_tool_map,
    ).ok()

    _expect_that_toolchain(
        env,
        known_features = [targets.implies_simple_feature],
        tool_map = targets.empty_tool_map,
        expr = "feature_missing_implies",
    ).err().contains(
        "%s implies the feature %s" % (targets.implies_simple_feature.label, targets.simple_feature.label),
    )

def _feature_missing_requirements_invalid_test(env, targets):
    _expect_that_toolchain(
        env,
        known_features = [targets.requires_any_simple_feature, targets.simple_feature],
        tool_map = targets.empty_tool_map,
        expr = "requires_any_simple_has_simple",
    ).ok()
    _expect_that_toolchain(
        env,
        known_features = [targets.requires_any_simple_feature, targets.simple_feature2],
        tool_map = targets.empty_tool_map,
        expr = "requires_any_simple_has_simple2",
    ).ok()
    _expect_that_toolchain(
        env,
        known_features = [targets.requires_any_simple_feature],
        tool_map = targets.empty_tool_map,
        expr = "requires_any_simple_has_none",
    ).err().contains(
        "It is impossible to enable %s" % targets.requires_any_simple_feature.label,
    )

    _expect_that_toolchain(
        env,
        known_features = [targets.requires_all_simple_feature, targets.simple_feature, targets.simple_feature2],
        tool_map = targets.empty_tool_map,
        expr = "requires_all_simple_has_both",
    ).ok()
    _expect_that_toolchain(
        env,
        known_features = [targets.requires_all_simple_feature, targets.simple_feature],
        tool_map = targets.empty_tool_map,
        expr = "requires_all_simple_has_simple",
    ).err().contains(
        "It is impossible to enable %s" % targets.requires_all_simple_feature.label,
    )
    _expect_that_toolchain(
        env,
        known_features = [targets.requires_all_simple_feature, targets.simple_feature2],
        tool_map = targets.empty_tool_map,
        expr = "requires_all_simple_has_simple2",
    ).err().contains(
        "It is impossible to enable %s" % targets.requires_all_simple_feature.label,
    )

def _args_missing_requirements_invalid_test(env, targets):
    _expect_that_toolchain(
        env,
        args = [targets.requires_all_simple_args],
        known_features = [targets.simple_feature, targets.simple_feature2],
        tool_map = targets.empty_tool_map,
        expr = "has_both",
    ).ok()
    _expect_that_toolchain(
        env,
        args = [targets.requires_all_simple_args],
        known_features = [targets.simple_feature],
        tool_map = targets.empty_tool_map,
        expr = "has_only_one",
    ).err().contains(
        "It is impossible to enable %s" % targets.requires_all_simple_args.label,
    )

def _toolchain_collects_files_test(env, targets):
    tc = env.expect.that_target(
        targets.collects_files_toolchain_config,
    ).provider(ToolchainConfigInfo)
    tc.files().get(targets.c_compile[ActionTypeInfo]).contains_exactly(_COLLECTED_C_COMPILE_FILES)
    tc.files().get(targets.cpp_compile[ActionTypeInfo]).contains_exactly(_COLLECTED_CPP_COMPILE_FILES)

    env.expect.that_target(
        targets.collects_files_c_compile,
    ).default_outputs().contains_exactly(_COLLECTED_C_COMPILE_FILES)
    env.expect.that_target(
        targets.collects_files_cpp_compile,
    ).default_outputs().contains_exactly(_COLLECTED_CPP_COMPILE_FILES)

    legacy = convert_toolchain(tc.actual)
    env.expect.that_collection(legacy.features).contains_exactly([
        legacy_feature(
            name = "simple_feature",
            enabled = True,
            flag_sets = [legacy_flag_set(
                actions = ["c_compile"],
                flag_groups = [
                    legacy_flag_group(flags = ["c_compile_args"]),
                ],
            )],
        ),
        legacy_feature(
            name = "compile_feature",
            enabled = False,
            flag_sets = [legacy_flag_set(
                actions = ["c_compile", "cpp_compile"],
                flag_groups = [
                    legacy_flag_group(flags = ["compile_args"]),
                ],
            )],
        ),
        legacy_feature(
            name = "supports_pic",
            enabled = False,
        ),
        legacy_feature(
            name = "implied_by_always_enabled_env_sets",
            enabled = True,
        ),
    ]).in_order()

    exe = tc.tool_map().some().configs().get(
        targets.c_compile[ActionTypeInfo],
    ).actual.exe
    env.expect.that_collection(legacy.action_configs).contains_exactly([
        legacy_action_config(
            action_name = "c_compile",
            enabled = True,
            tools = [legacy_tool(tool = exe)],
            implies = ["supports_pic"],
            flag_sets = [
                legacy_flag_set(
                    flag_groups = [
                        legacy_flag_group(flags = [
                            "--sysroot=tests/rule_based_toolchain/testdata",
                        ]),
                    ],
                ),
                legacy_flag_set(
                    flag_groups = [
                        legacy_flag_group(flags = ["c_compile_args"]),
                    ],
                ),
            ],
        ),
        legacy_action_config(
            action_name = "cpp_compile",
            enabled = True,
            tools = [legacy_tool(tool = exe)],
            implies = [],
            flag_sets = [
                legacy_flag_set(
                    flag_groups = [
                        legacy_flag_group(flags = [
                            "--sysroot=tests/rule_based_toolchain/testdata",
                        ]),
                    ],
                ),
            ],
        ),
    ]).in_order()

TARGETS = [
    "//tests/rule_based_toolchain/actions:c_compile",
    "//tests/rule_based_toolchain/actions:cpp_compile",
    ":builtin_feature",
    ":compile_tool_map",
    ":collects_files_c_compile",
    ":collects_files_cpp_compile",
    ":collects_files_toolchain_config",
    ":compile_feature",
    ":c_compile_args",
    ":c_compile_tool_map",
    ":empty_tool_map",
    ":implies_simple_feature",
    ":overrides_feature",
    ":requires_any_simple_feature",
    ":requires_all_simple_feature",
    ":requires_all_simple_args",
    ":simple_feature",
    ":simple_feature2",
    ":same_feature_name",
]

# @unsorted-dict-items
TESTS = {
    "empty_toolchain_valid_test": _empty_toolchain_valid_test,
    "missing_tool_map_invalid_test": _missing_tool_map_invalid_test,
    "duplicate_feature_names_fail_validation_test": _duplicate_feature_names_invalid_test,
    "feature_config_implies_missing_feature_invalid_test": _feature_config_implies_missing_feature_invalid_test,
    "feature_missing_requirements_invalid_test": _feature_missing_requirements_invalid_test,
    "args_missing_requirements_invalid_test": _args_missing_requirements_invalid_test,
    "toolchain_collects_files_test": _toolchain_collects_files_test,
}
