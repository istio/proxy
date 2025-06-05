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
"""Implementation of the cc_toolchain rule."""

load("//cc/common:cc_common.bzl", "cc_common")
load(
    "//cc/toolchains:cc_toolchain_info.bzl",
    "ActionTypeSetInfo",
    "ArgsListInfo",
    "FeatureSetInfo",
    "ToolConfigInfo",
    "ToolchainConfigInfo",
)
load(":collect.bzl", "collect_action_types")
load(":legacy_converter.bzl", "convert_toolchain")
load(":toolchain_config_info.bzl", "toolchain_config_info")

visibility([
    "//cc/toolchains/...",
    "//tests/rule_based_toolchain/...",
])

def _cc_legacy_file_group_impl(ctx):
    files = ctx.attr.config[ToolchainConfigInfo].files

    return [DefaultInfo(files = depset(transitive = [
        files[action]
        for action in collect_action_types(ctx.attr.actions).to_list()
        if action in files
    ]))]

cc_legacy_file_group = rule(
    implementation = _cc_legacy_file_group_impl,
    attrs = {
        "actions": attr.label_list(providers = [ActionTypeSetInfo], mandatory = True),
        "config": attr.label(providers = [ToolchainConfigInfo], mandatory = True),
    },
)

def _cc_toolchain_config_impl(ctx):
    if ctx.attr.features:
        fail("Features is a reserved attribute in bazel. Did you mean 'known_features' or 'enabled_features'?")

    toolchain_config = toolchain_config_info(
        label = ctx.label,
        known_features = ctx.attr.known_features + [ctx.attr._builtin_features],
        enabled_features = ctx.attr.enabled_features,
        tool_map = ctx.attr.tool_map,
        args = ctx.attr.args,
    )

    legacy = convert_toolchain(toolchain_config)

    return [
        toolchain_config,
        cc_common.create_cc_toolchain_config_info(
            ctx = ctx,
            action_configs = legacy.action_configs,
            features = legacy.features,
            cxx_builtin_include_directories = legacy.cxx_builtin_include_directories,
            # toolchain_identifier is deprecated, but setting it to None results
            # in an error that it expected a string, and for safety's sake, I'd
            # prefer to provide something unique.
            toolchain_identifier = str(ctx.label),
            # This can be accessed by users through
            # @rules_cc//cc/private/toolchain:compiler to select() on the current
            # compiler
            compiler = ctx.attr.compiler,
            # These fields are only relevant for legacy toolchain resolution.
            target_system_name = "",
            target_cpu = "",
            target_libc = "",
            abi_version = "",
            abi_libc_version = "",
        ),
        # This allows us to support all_files.
        # If all_files was simply an alias to
        # //cc/toolchains/actions:all_actions,
        # then if a toolchain introduced a new type of action, it wouldn't get
        # put in all_files.
        DefaultInfo(files = depset(transitive = toolchain_config.files.values())),
    ]

cc_toolchain_config = rule(
    implementation = _cc_toolchain_config_impl,
    # @unsorted-dict-items
    attrs = {
        # Attributes new to this rule.
        "compiler": attr.string(default = ""),
        "tool_map": attr.label(providers = [ToolConfigInfo], mandatory = True),
        "args": attr.label_list(providers = [ArgsListInfo]),
        "known_features": attr.label_list(providers = [FeatureSetInfo]),
        "enabled_features": attr.label_list(providers = [FeatureSetInfo]),
        "_builtin_features": attr.label(default = "//cc/toolchains/features:all_builtin_features"),
    },
    provides = [ToolchainConfigInfo],
)
