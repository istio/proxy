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
"""This is a list of rules/macros that should be exported as documentation."""

load("//cc/toolchains:actions.bzl", _cc_action_type = "cc_action_type", _cc_action_type_set = "cc_action_type_set")
load("//cc/toolchains:args.bzl", _cc_args = "cc_args")
load("//cc/toolchains:args_list.bzl", _cc_args_list = "cc_args_list")
load("//cc/toolchains:feature.bzl", _cc_feature = "cc_feature")
load("//cc/toolchains:feature_constraint.bzl", _cc_feature_constraint = "cc_feature_constraint")
load("//cc/toolchains:feature_set.bzl", _cc_feature_set = "cc_feature_set")
load("//cc/toolchains:mutually_exclusive_category.bzl", _cc_mutually_exclusive_category = "cc_mutually_exclusive_category")
load("//cc/toolchains:nested_args.bzl", _cc_nested_args = "cc_nested_args")
load("//cc/toolchains:tool.bzl", _cc_tool = "cc_tool")
load("//cc/toolchains:tool_capability.bzl", _cc_tool_capability = "cc_tool_capability")
load("//cc/toolchains:tool_map.bzl", _cc_tool_map = "cc_tool_map")
load("//cc/toolchains:toolchain.bzl", _cc_toolchain = "cc_toolchain")
load("//cc/toolchains/impl:external_feature.bzl", _cc_external_feature = "cc_external_feature")
load("//cc/toolchains/impl:variables.bzl", _cc_variable = "cc_variable")

cc_tool_map = _cc_tool_map
cc_tool = _cc_tool
cc_tool_capability = _cc_tool_capability
cc_args = _cc_args
cc_nested_args = _cc_nested_args
cc_args_list = _cc_args_list
cc_action_type = _cc_action_type
cc_action_type_set = _cc_action_type_set
cc_variable = _cc_variable
cc_feature = _cc_feature
cc_feature_constraint = _cc_feature_constraint
cc_feature_set = _cc_feature_set
cc_mutually_exclusive_category = _cc_mutually_exclusive_category
cc_external_feature = _cc_external_feature
cc_toolchain = _cc_toolchain

# This list is used to automatically remap instances of `foo` to [`foo`](#foo)
# links in the generated documentation so that maintainers don't need to manually
# ensure every reference to a rule is properly linked.
DOCUMENTED_TOOLCHAIN_RULES = [
    "cc_tool_map",
    "cc_tool",
    "cc_tool_capability",
    "cc_args",
    "cc_nested_args",
    "cc_args_list",
    "cc_action_type",
    "cc_action_type_set",
    "cc_variable",
    "cc_feature",
    "cc_feature_constraint",
    "cc_feature_set",
    "cc_mutually_exclusive_category",
    "cc_external_feature",
    "cc_toolchain",
]
