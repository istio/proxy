# Copyright 2022 The Bazel Authors. All rights reserved.
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

"""Definitions used to configure toolchain actions."""

load("@bazel_skylib//lib:types.bzl", "types")

def _normalize_action_config_features(features):
    """Validates and normalizes the `features` of an `action_config`.

    This method validates that the argument is either `None`, a non-empty
    list of strings, or a non-empty list of lists of strings. If the argument is
    the shorthand form (a list of strings), it is normalized by wrapping it in
    an outer list so that action building code does not need to be concerned
    about the distinction.

    Args:
        features: The `features` argument passed to `action_config`.

    Returns:
        The `features` argument, normalized if necessary.
    """
    if features == None:
        return features

    failure_message = (
        "The 'features' argument passed to " +
        "'swift_toolchain_config.action_config' must be either None, a list " +
        "of strings, or a list of lists of strings.",
    )

    # Fail if the argument is not a list, or if it is but it is empty.
    if not types.is_list(features) or not features:
        fail(failure_message)

    outer_list_has_strings = False
    outer_list_has_lists = False

    # Check each element in the list to determine if it is a list of lists
    # or a list of strings.
    for element in features:
        if types.is_list(element):
            outer_list_has_lists = True
        elif types.is_string(element) and element:
            outer_list_has_strings = True
        else:
            fail(failure_message)

    # Forbid mixing lists and strings at the top-level.
    if outer_list_has_strings and outer_list_has_lists:
        fail(failure_message)

    # If the original list was a list of strings, wrap it before returning it
    # to the caller.
    if outer_list_has_strings:
        return [features]

    # Otherwise, return the original list of lists.
    return features

def _action_config_init(
        *,
        actions,
        configurators,
        features = None,
        not_features = None):
    """Validates and initializes a new Swift toolchain action configuration.

    This function validates the inputs, causing the build to fail if they have
    incorrect types or are otherwise invalid.

    Args:
        actions: A `list` of strings denoting the names of the actions for
            which the configurators should be invoked.
        configurators: A `list` of functions or that will be invoked to add
            command line arguments and collect inputs for the actions. These
            functions take two arguments---a `prerequisites` struct and an
            `Args` object---and return a either a struct via `config_result`
            that describes that `File`s that should be used as inputs to the
            action, or `None` if the configurator does not add any inputs.
        features: The `list` of features that must be enabled for the
            configurators to be applied to the action. This argument can take
            one of three forms: `None` (the default), in which case the
            configurators are unconditionally applied; a non-empty `list` of
            `list`s of feature names (strings), in which case *all* features
            mentioned in *one* of the inner lists must be enabled; or a single
            non-empty `list` of feature names, which is a shorthand form
            equivalent to that single list wrapped in another list.
        not_features: The `list` of features that must be disabled for the
            configurators to be applied to the action. Like `features`, this
            argument can take one of three forms: `None` (the default), in
            which case the configurators are applied if `features` was
            satisfied; a non-empty `list` of `list`s of feature names (strings),
            in which case *all* features mentioned in *one* of the inner lists
            must be disabled, otherwise the configurators will not be applied,
            even if `features` was satisfied; or a single non-empty `list` of
            feature names, which is a shorthand form equivalent to that single
            list wrapped in another list.

    Returns:
        A validated action configuration.
    """
    return {
        "actions": actions,
        "configurators": configurators,
        "features": _normalize_action_config_features(features),
        "not_features": _normalize_action_config_features(not_features),
    }

def _config_result_init(
        *,
        additional_tools = [],
        inputs = [],
        transitive_inputs = []):
    """Validates and initializes an action configurator result.

    Args:
        additional_tools: A list of `depset`s of `File`s that should be passed
            as additional tool inputs to the action being configured.
        inputs: A list of `File`s that should be passed as inputs to the action
            being configured.
        transitive_inputs: A list of `depset`s of `File`s that should be passed
            as inputs to the action being configured.

    Returns:
        A new config result that can be returned from a configurator.
    """
    return {
        "additional_tools": additional_tools,
        "inputs": inputs,
        "transitive_inputs": transitive_inputs,
    }

def add_arg(arg_name_or_value, value = None, format = None):
    """Returns a configurator that adds a simple argument to the command line.

    This is provided as a convenience for the simple case where a configurator
    wishes to add a flag to the command line, perhaps based on the enablement
    of a feature, without writing a separate function solely for that one flag.

    Args:
        arg_name_or_value: The `arg_name_or_value` argument that will be passed
            to `Args.add`.
        value: The `value` argument that will be passed to `Args.add` (`None`
            by default).
        format: The `format` argument that will be passed to `Args.add` (`None`
            by default).

    Returns:
        A function that can be added to the `configurators` list of an
        `action_config`.
    """

    # `Args.add` doesn't permit the `value` argument to be `None`, only
    # "unbound", so we have to check for this and not pass it *at all* if it
    # wasn't specified when the function was created.
    if value == None:
        return lambda _prerequisites, args: args.add(
            arg_name_or_value,
            format = format,
        )

    return lambda _prerequisites, args: args.add(
        arg_name_or_value,
        value,
        format = format,
    )

ActionConfigInfo, _action_config_init_unchecked = provider(
    doc = "An action configuration in the Swift toolchain.",
    fields = [
        "actions",
        "configurators",
        "features",
        "not_features",
    ],
    init = _action_config_init,
)

ConfigResultInfo, _config_result_init_unchecked = provider(
    doc = "The inputs required by an action configurator.",
    fields = [
        "additional_tools",  # List[depset[File]]
        "inputs",  # list[File]
        "transitive_inputs",  # List[depset[File]]
    ],
    init = _config_result_init,
)
