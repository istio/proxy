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
"""Helper functions to create and validate a ToolchainConfigInfo."""

load("//cc/toolchains:cc_toolchain_info.bzl", "ToolConfigInfo", "ToolchainConfigInfo")
load(":args_utils.bzl", "get_action_type")
load(":collect.bzl", "collect_args_lists", "collect_features")

visibility([
    "//cc/toolchains/...",
    "//tests/rule_based_toolchain/...",
])

_FEATURE_NAME_ERR = """The feature name {name} was defined by both {lhs} and {rhs}.

Possible causes:
* If you're overriding a feature in //cc/toolchains/features/..., then try adding the "overrides" parameter instead of specifying a feature name.
* If you intentionally have multiple features with the same name (eg. one for ARM and one for x86), then maybe you need add select() calls so that they're not defined at the same time.
* Otherwise, this is probably a real problem, and you need to give them different names.
"""

_INVALID_CONSTRAINT_ERR = """It is impossible to enable {provider}.

None of the entries in requires_any_of could be matched. This is required features are not implicitly added to the toolchain. It's likely that the feature that you require needs to be added to the toolchain explicitly.
"""

_UNKNOWN_FEATURE_ERR = """{self} implies the feature {ft}, which was unable to be found.

Implied features are not implicitly added to your toolchain. You likely need to add features = ["{ft}"] to your cc_toolchain rule.
"""

# Equality comparisons with bazel do not evaluate depsets.
# s = struct()
# d = depset([s])
# depset([s]) != depset([s])
# d == d
# This means that complex structs such as FeatureInfo will only compare as equal
# iff they are the *same* object or if there are no depsets inside them.
# Unfortunately, it seems that the FeatureInfo is copied during the
# cc_action_type_config rule. Ideally we'd like to fix that, but I don't really
# know what power we even have over such a thing.
def _feature_key(feature):
    # This should be sufficiently unique.
    return (feature.label, feature.name)

def _get_known_features(features, capability_features, fail):
    feature_names = {}
    for ft in capability_features + features:
        if ft.name in feature_names:
            other = feature_names[ft.name]
            if other.overrides != ft and ft.overrides != other:
                fail(_FEATURE_NAME_ERR.format(
                    name = ft.name,
                    lhs = ft.label,
                    rhs = other.label,
                ))
        feature_names[ft.name] = ft

    return {_feature_key(feature): None for feature in features}

def _can_theoretically_be_enabled(requirement, known_features):
    return all([
        _feature_key(ft) in known_features
        for ft in requirement
    ])

def _validate_requires_any_of(fn, self, known_features, fail):
    valid = any([
        _can_theoretically_be_enabled(fn(requirement), known_features)
        for requirement in self.requires_any_of
    ])

    # No constraints is always valid.
    if self.requires_any_of and not valid:
        fail(_INVALID_CONSTRAINT_ERR.format(provider = self.label))

def _validate_requires_any_of_feature_set(self, known_features, fail):
    return _validate_requires_any_of(
        lambda feature_set: feature_set.features.to_list(),
        self,
        known_features,
        fail,
    )

def _validate_implies(self, known_features, fail = fail):
    for ft in self.implies.to_list():
        if _feature_key(ft) not in known_features:
            fail(_UNKNOWN_FEATURE_ERR.format(self = self.label, ft = ft.label))

def _validate_args(self, known_features, fail):
    return _validate_requires_any_of(
        lambda constraint: constraint.all_of.to_list(),
        self,
        known_features,
        fail,
    )

def _validate_feature(self, known_features, fail):
    _validate_requires_any_of_feature_set(self, known_features, fail = fail)
    for arg in self.args.args:
        _validate_args(arg, known_features, fail = fail)
    _validate_implies(self, known_features, fail = fail)

def _validate_toolchain(self, fail = fail):
    capabilities = []
    for tool in self.tool_map.configs.values():
        capabilities.extend([cap.feature for cap in tool.capabilities])
    known_features = _get_known_features(self.features, capabilities, fail = fail)

    for feature in self.features:
        _validate_feature(feature, known_features, fail = fail)
    for args in self.args.args:
        _validate_args(args, known_features, fail = fail)

def _collect_files_for_action_type(action_type, tool_map, features, args):
    transitive_files = [tool_map[action_type].runfiles.files, get_action_type(args, action_type).files]
    for ft in features:
        transitive_files.append(get_action_type(ft.args, action_type).files)

    return depset(transitive = transitive_files)

def toolchain_config_info(label, known_features = [], enabled_features = [], args = [], tool_map = None, fail = fail):
    """Generates and validates a ToolchainConfigInfo from lists of labels.

    Args:
        label: (Label) The label to apply to the ToolchainConfigInfo
        known_features: (List[Target]) A list of features that can be enabled.
        enabled_features: (List[Target]) A list of features that are enabled by
          default. Every enabled feature is implicitly also a known feature.
        args: (List[Target]) A list of targets providing ArgsListInfo
        tool_map: (Target) A target providing ToolMapInfo.
        fail: A fail function. Use only during tests.
    Returns:
        A validated ToolchainConfigInfo
    """

    # Later features will come after earlier features on the command-line, and
    # thus override them. Because of this, we ensure that known_features comes
    # *after* enabled_features, so that if we do enable them, they override the
    # default feature flags.
    features = collect_features(enabled_features + known_features).to_list()
    enabled_features = collect_features(enabled_features).to_list()

    if tool_map == None:
        fail("tool_map is required")

        # The `return` here is to support testing, since injecting `fail()` has a
        # side-effect of allowing code to continue.
        return None  # buildifier: disable=unreachable

    args = collect_args_lists(args, label = label)
    tools = tool_map[ToolConfigInfo].configs
    files = {
        action_type: _collect_files_for_action_type(action_type, tools, features, args)
        for action_type in tools.keys()
    }
    allowlist_include_directories = depset(
        transitive = [
            src.allowlist_include_directories
            for src in features + tools.values()
        ] + [args.allowlist_include_directories],
    )
    toolchain_config = ToolchainConfigInfo(
        label = label,
        features = features,
        enabled_features = enabled_features,
        tool_map = tool_map[ToolConfigInfo],
        args = args,
        files = files,
        allowlist_include_directories = allowlist_include_directories,
    )
    _validate_toolchain(toolchain_config, fail = fail)
    return toolchain_config
