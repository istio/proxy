# Copyright 2024 The Bazel Authors. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""All providers for rule-based bazel toolchain config."""

# Until the providers are stabilized, ensure that rules_cc is the only place
# that can access the providers directly.
# Once it's stabilized, we *may* consider opening up parts of the API, or we may
# decide to just require users to use the public user-facing rules.
visibility([
    "//cc/toolchains/...",
    "//tests/rule_based_toolchain/...",
])

# Note that throughout this file, we never use a list. This is because mutable
# types cannot be stored in depsets. Thus, we type them as a sequence in the
# provider, and convert them to a tuple in the constructor to ensure
# immutability.

ActionTypeInfo = provider(
    doc = "A type of action (eg. c-compile, c++-link-executable)",
    # @unsorted-dict-items
    fields = {
        "label": "(Label) The label defining this provider. Place in error messages to simplify debugging",
        "name": "(str) The action name, as defined by action_names.bzl",
    },
)

ActionTypeSetInfo = provider(
    doc = "A set of types of actions",
    # @unsorted-dict-items
    fields = {
        "label": "(Label) The label defining this provider. Place in error messages to simplify debugging",
        "actions": "(depset[ActionTypeInfo]) Set of action types",
    },
)

VariableInfo = provider(
    """A variable defined by the toolchain""",
    # @unsorted-dict-items
    fields = {
        "name": "(str) The variable name",
        "label": "(Label) The label defining this provider. Place in error messages to simplify debugging",
        "actions": "(Optional[depset[ActionTypeInfo]]) The actions this variable is available for",
        "type": "A type constructed using variables.types.*",
    },
)

BuiltinVariablesInfo = provider(
    doc = "The builtin variables",
    fields = {
        "variables": "(dict[str, VariableInfo]) A mapping from variable name to variable metadata.",
    },
)

NestedArgsInfo = provider(
    doc = "A provider representation of Args.add/add_all/add_joined parameters",
    # @unsorted-dict-items
    fields = {
        "label": "(Label) The label defining this provider. Place in error messages to simplify debugging",
        "nested": "(Sequence[NestedArgsInfo]) The nested arg expansion. Mutually exclusive with args",
        "iterate_over": "(Optional[str]) The variable to iterate over",
        "files": "(depset[File]) The files required to use this variable",
        "requires_types": "(dict[str, str]) A mapping from variables to their expected type name (not type). This means that we can require the generic type Option, rather than an Option[T]",
        "legacy_flag_group": "(flag_group) The flag_group this corresponds to",
        "unwrap_options": "(List[str]) A list of variables for which we should unwrap the option. For example, if a user writes `requires_not_none = \":foo\"`, then we change the type of foo from Option[str] to str",
    },
)

ArgsInfo = provider(
    doc = "A set of arguments to be added to the command line for specific actions",
    # @unsorted-dict-items
    fields = {
        "label": "(Label) The label defining this provider. Place in error messages to simplify debugging",
        "actions": "(depset[ActionTypeInfo]) The set of actions this is associated with",
        "requires_any_of": "(Sequence[FeatureConstraintInfo]) This will be enabled if any of the listed predicates are met. Equivalent to with_features",
        "nested": "(Optional[NestedArgsInfo]) The args expand. Equivalent to a flag group.",
        "files": "(depset[File]) Files required for the args",
        "env": "(dict[str, str]) Environment variables to apply",
        "allowlist_include_directories": "(depset[DirectoryInfo]) Include directories implied by these arguments that should be allowlisted in Bazel's include checker",
    },
)
ArgsListInfo = provider(
    doc = "A ordered list of arguments",
    # @unsorted-dict-items
    fields = {
        "label": "(Label) The label defining this provider. Place in error messages to simplify debugging",
        "args": "(Sequence[ArgsInfo]) The flag sets contained within",
        "files": "(depset[File]) The files required for all of the arguments",
        "by_action": "(Sequence[struct(action=ActionTypeInfo, args=List[ArgsInfo], files=depset[Files])]) Relevant information about the args keyed by the action type.",
        "allowlist_include_directories": "(depset[DirectoryInfo]) Include directories implied by these arguments that should be allowlisted in Bazel's include checker",
    },
)

FeatureInfo = provider(
    doc = "Contains all flag specifications for one feature.",
    # @unsorted-dict-items
    fields = {
        "label": "(Label) The label defining this provider. Place in error messages to simplify debugging",
        "name": "(str) The name of the feature",
        "enabled": "(bool) Whether this feature is enabled by default",
        "args": "(ArgsListInfo) Args enabled by this feature",
        "implies": "(depset[FeatureInfo]) Set of features implied by this feature",
        "requires_any_of": "(Sequence[FeatureSetInfo]) A list of feature sets, at least one of which is required to enable this feature. This is semantically equivalent to the requires attribute of rules_cc's FeatureInfo",
        "mutually_exclusive": "(Sequence[MutuallyExclusiveCategoryInfo]) Indicates that this feature is one of several mutually exclusive alternate features.",
        "external": "(bool) Whether a feature is defined elsewhere.",
        "overridable": "(bool) Whether the feature is an overridable feature.",
        "overrides": "(Optional[FeatureInfo]) The feature that this overrides. Must be a known feature",
        "allowlist_include_directories": "(depset[DirectoryInfo]) Include directories implied by this feature that should be allowlisted in Bazel's include checker",
    },
)
FeatureSetInfo = provider(
    doc = "A set of features",
    # @unsorted-dict-items
    fields = {
        "label": "(Label) The label defining this provider. Place in error messages to simplify debugging",
        "features": "(depset[FeatureInfo]) The set of features this corresponds to",
    },
)

FeatureConstraintInfo = provider(
    doc = "A predicate checking that certain features are enabled and others disabled.",
    # @unsorted-dict-items
    fields = {
        "label": "(Label) The label defining this provider. Place in error messages to simplify debugging",
        "all_of": "(depset[FeatureInfo]) A set of features which must be enabled",
        "none_of": "(depset[FeatureInfo]) A set of features, none of which can be enabled",
    },
)

MutuallyExclusiveCategoryInfo = provider(
    doc = "Multiple features with the category will be mutally exclusive",
    # @unsorted-dict-items
    fields = {
        "label": "(Label) The label defining this provider. Place in error messages to simplify debugging",
        "name": "(str) The name of the category",
    },
)

ToolInfo = provider(
    doc = "A binary, with additional metadata to make it useful for action configs.",
    # @unsorted-dict-items
    fields = {
        "label": "(Label) The label defining this provider. Place in error messages to simplify debugging",
        "exe": "(File) The file corresponding to the tool",
        "runfiles": "(runfiles) The files required to run the tool",
        "execution_requirements": "(Sequence[str]) A set of execution requirements of the tool",
        "allowlist_include_directories": "(depset[DirectoryInfo]) Built-in include directories implied by this tool that should be allowlisted in Bazel's include checker",
        "capabilities": "(Sequence[ToolCapabilityInfo]) Capabilities supported by the tool.",
    },
)

ToolCapabilityInfo = provider(
    doc = "A capability associated with a tool (eg. supports_pic).",
    # @unsorted-dict-items
    fields = {
        "label": "(Label) The label defining this provider. Place in error messages to simplify debugging",
        "feature": "(FeatureInfo) The feature this capability defines",
    },
)

ToolConfigInfo = provider(
    doc = "A mapping from action to tool",
    # @unsorted-dict-items
    fields = {
        "label": "(Label) The label defining this provider. Place in error messages to simplify debugging",
        "configs": "(dict[ActionTypeInfo, ToolInfo]) A mapping from action to tool.",
    },
)

ToolchainConfigInfo = provider(
    doc = "The configuration for a toolchain",
    # @unsorted-dict-items
    fields = {
        "label": "(Label) The label defining this provider. Place in error messages to simplify debugging",
        "features": "(Sequence[FeatureInfo]) The features available for this toolchain",
        "enabled_features": "(Sequence[FeatureInfo]) The features That are enabled by default for this toolchain",
        "tool_map": "(ToolConfigInfo) A provider mapping toolchain action types to tools.",
        "args": "(Sequence[ArgsInfo]) A list of arguments to be unconditionally applied to the toolchain.",
        "files": "(dict[ActionTypeInfo, depset[File]]) Files required for the toolchain, keyed by the action type.",
        "allowlist_include_directories": "(depset[DirectoryInfo]) Built-in include directories implied by this toolchain's args and tools that should be allowlisted in Bazel's include checker",
    },
)
