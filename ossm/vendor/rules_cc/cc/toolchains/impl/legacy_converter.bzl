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
"""Conversion helper functions to legacy cc_toolchain_config_info."""

load(
    "//cc:cc_toolchain_config_lib.bzl",
    legacy_action_config = "action_config",
    legacy_env_entry = "env_entry",
    legacy_env_set = "env_set",
    legacy_feature = "feature",
    legacy_feature_set = "feature_set",
    legacy_flag_set = "flag_set",
    legacy_tool = "tool",
    legacy_with_feature_set = "with_feature_set",
)

visibility([
    "//cc/toolchains/...",
    "//tests/rule_based_toolchain/...",
])

# Note that throughout this file, we sort anything for which the order is
# nondeterministic (eg. depset's .to_list(), dictionary iteration).
# This allows our tests to call equals() on the output,
# and *may* provide better caching properties.

def _convert_actions(actions):
    return sorted([action.name for action in actions.to_list()])

def convert_feature_constraint(constraint):
    return legacy_with_feature_set(
        features = sorted([ft.name for ft in constraint.all_of.to_list()]),
        not_features = sorted([ft.name for ft in constraint.none_of.to_list()]),
    )

def convert_args(args, strip_actions = False):
    """Converts an ArgsInfo to flag_sets and env_sets.

    Args:
        args: (ArgsInfo) The args to convert
        strip_actions: (bool) Whether to strip the actions from the resulting flag_set.
    Returns:
        struct(flag_sets = List[flag_set], env_sets = List[env_sets])
    """
    actions = _convert_actions(args.actions)
    with_features = [
        convert_feature_constraint(fc)
        for fc in args.requires_any_of
    ]

    flag_sets = []
    if args.nested != None:
        flag_sets.append(legacy_flag_set(
            actions = [] if strip_actions else actions,
            with_features = with_features,
            flag_groups = [args.nested.legacy_flag_group],
        ))

    env_sets = []
    if args.env:
        env_sets.append(legacy_env_set(
            actions = actions,
            with_features = with_features,
            env_entries = [
                legacy_env_entry(
                    key = key,
                    value = value,
                )
                for key, value in args.env.items()
            ],
        ))
    return struct(
        flag_sets = flag_sets,
        env_sets = env_sets,
    )

def _convert_args_sequence(args_sequence, strip_actions = False):
    flag_sets = []
    env_sets = []
    for args in args_sequence:
        legacy_args = convert_args(args, strip_actions)
        flag_sets.extend(legacy_args.flag_sets)
        env_sets.extend(legacy_args.env_sets)

    return struct(flag_sets = flag_sets, env_sets = env_sets)

def convert_feature(feature, enabled = False):
    if feature.external:
        return None

    args = _convert_args_sequence(feature.args.args)

    return legacy_feature(
        name = feature.name,
        enabled = enabled or feature.enabled,
        flag_sets = args.flag_sets,
        env_sets = args.env_sets,
        implies = sorted([ft.name for ft in feature.implies.to_list()]),
        requires = [
            legacy_feature_set(sorted([
                feature.name
                for feature in requirement.features.to_list()
            ]))
            for requirement in feature.requires_any_of
        ],
        provides = [
            mutex.name
            for mutex in feature.mutually_exclusive
        ],
    )

def convert_tool(tool):
    return legacy_tool(
        tool = tool.exe,
        execution_requirements = list(tool.execution_requirements),
        with_features = [],
    )

def convert_capability(capability):
    return legacy_feature(
        name = capability.name,
        enabled = False,
    )

def _convert_tool_map(tool_map, args_by_action):
    action_configs = []
    caps = {}
    for action_type, tool in tool_map.configs.items():
        action_args = args_by_action.get(action_type.name, default = None)
        flag_sets = action_args.flag_sets if action_args != None else []
        action_configs.append(legacy_action_config(
            action_name = action_type.name,
            enabled = True,
            flag_sets = flag_sets,
            tools = [convert_tool(tool)],
            implies = [cap.feature.name for cap in tool.capabilities],
        ))
        for cap in tool.capabilities:
            caps[cap] = None

    cap_features = [
        legacy_feature(name = cap.feature.name, enabled = False)
        for cap in caps
    ]
    return action_configs, cap_features

def convert_toolchain(toolchain):
    """Converts a rule-based toolchain into the legacy providers.

    Args:
        toolchain: (ToolchainConfigInfo) The toolchain config to convert.
    Returns:
        A struct containing parameters suitable to pass to
          cc_common.create_cc_toolchain_config_info.
    """

    # Ordering of arguments is important! Intended argument ordering is:
    #   1. Arguments listed in `args`.
    #   2. Legacy/built-in features.
    #   3. User-defined features.
    # While we could just attach arguments to a feature, legacy/built-in features will appear
    # before the user-defined features if we do not bind args directly to the action configs.
    # For that reason, there's additional logic in this function to ensure that the args are
    # attached to the action configs directly, as that is the only way to ensure the correct
    # ordering.
    args_by_action = {}
    for a in toolchain.args.by_action:
        args = args_by_action.setdefault(a.action.name, struct(flag_sets = [], env_sets = []))
        new_args = _convert_args_sequence(a.args, strip_actions = True)
        args.flag_sets.extend(new_args.flag_sets)
        args.env_sets.extend(new_args.env_sets)

    action_configs, cap_features = _convert_tool_map(toolchain.tool_map, args_by_action)
    features = [
        convert_feature(feature, enabled = feature in toolchain.enabled_features)
        for feature in toolchain.features
    ]
    features.extend(cap_features)

    features.append(legacy_feature(
        # We reserve names starting with implied_by. This ensures we don't
        # conflict with the name of a feature the user creates.
        name = "implied_by_always_enabled_env_sets",
        enabled = True,
        env_sets = _convert_args_sequence(toolchain.args.args).env_sets,
    ))

    cxx_builtin_include_directories = [
        d.path
        for d in toolchain.allowlist_include_directories.to_list()
    ]

    return struct(
        features = [ft for ft in features if ft != None],
        action_configs = sorted(action_configs, key = lambda ac: ac.action_name),
        cxx_builtin_include_directories = cxx_builtin_include_directories,
    )
