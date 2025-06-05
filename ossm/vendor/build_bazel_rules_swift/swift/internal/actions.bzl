# Copyright 2018 The Bazel Authors. All rights reserved.
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

"""Functions for registering actions that invoke Swift tools."""

load("@bazel_skylib//lib:types.bzl", "types")
load("//swift/toolchains/config:action_config.bzl", "ConfigResultInfo")
load(":features.bzl", "are_all_features_enabled")

# This is a proxy for being on bazel 7.x which has
# --incompatible_merge_fixed_and_default_shell_env enabled by default
USE_DEFAULT_SHELL_ENV = not hasattr(apple_common, "apple_crosstool_transition")

def _apply_action_configs(
        action_name,
        args,
        feature_configuration,
        prerequisites,
        swift_toolchain):
    """Applies the action configs for the given action.

    Args:
        action_name: The name of the action that should be run.
        args: The `Args` object to which command line flags will be added.
        feature_configuration: A feature configuration obtained from
            `configure_features`.
        prerequisites: An action-specific `struct` whose fields can be accessed
            by the action configurators to add files and other dependent data to
            the command line.
        swift_toolchain: The Swift toolchain being used to build.

    Returns:
        A `ConfigResultInfo` value that contains the files that are required
        inputs of the action, as determined by the configurators.
    """
    additional_tools = []
    inputs = []
    transitive_inputs = []

    for action_config in swift_toolchain.action_configs:
        # Skip the action config if it does not apply to the requested action.
        if action_name not in action_config.actions:
            continue

        if action_config.features == None:
            # If the feature list was `None`, unconditionally apply the
            # configurators.
            should_apply_configurators = True
        else:
            # Check each of the feature lists to determine if any of them has
            # all of its features satisfied by the feature configuration.
            should_apply_configurators = False
            for feature_names in action_config.features:
                if are_all_features_enabled(
                    feature_configuration = feature_configuration,
                    feature_names = feature_names,
                ):
                    should_apply_configurators = True
                    break

        # If we should apply the configurators so far but there are exclusionary
        # features, check those as well and possibly decide to not apply the
        # configurators based on those.
        if should_apply_configurators and action_config.not_features:
            # The configurators will not be applied if any of the
            # `not_features` exclusion lists are entirely enabled.
            for feature_names in action_config.not_features:
                if are_all_features_enabled(
                    feature_configuration = feature_configuration,
                    feature_names = feature_names,
                ):
                    should_apply_configurators = False
                    break

        if not should_apply_configurators:
            continue

        # If one of the feature lists is completely satisfied, invoke the
        # configurators.
        for configurator in action_config.configurators:
            action_inputs = configurator(prerequisites, args)

            # If we create an action configurator from a lambda that calls
            # `Args.add*`, the result will be the `Args` objects (rather than
            # `None`) because those methods return the same `Args` object for
            # chaining. We can guard against this (and possibly other errors) by
            # checking that the value is a struct. If it is, then it's not
            # `None` and it probably came from the provider used by
            # `ConfigResultInfo`. If it's some other kind of struct, then we'll
            # error out trying to access the fields.
            if not type(action_inputs) == "struct":
                continue

            additional_tools.extend(action_inputs.additional_tools)
            inputs.extend(action_inputs.inputs)
            transitive_inputs.extend(action_inputs.transitive_inputs)

    # Merge the action results into a single result that we return.
    return ConfigResultInfo(
        additional_tools = additional_tools,
        inputs = inputs,
        transitive_inputs = transitive_inputs,
    )

def is_action_enabled(action_name, swift_toolchain):
    """Returns True if the given action is enabled in the Swift toolchain.

    Args:
        action_name: The name of the action.
        swift_toolchain: The Swift toolchain being used to build.

    Returns:
        True if the action is enabled, or False if it is not.
    """
    tool_config = swift_toolchain.tool_configs.get(action_name)
    return bool(tool_config)

def run_toolchain_action(
        *,
        actions,
        action_name,
        exec_group = None,
        feature_configuration,
        prerequisites,
        swift_toolchain,
        mnemonic = None,
        **kwargs):
    """Runs an action using the toolchain's tool and action configurations.

    Args:
        actions: The rule context's `Actions` object, which will be used to
            create `Args` objects.
        action_name: The name of the action that should be run.
        exec_group: Runs the Swift compilation action under the given execution
            group's context. If `None`, the default execution group is used.
        feature_configuration: A feature configuration obtained from
            `configure_features`.
        mnemonic: The mnemonic to associate with the action. If not provided,
            the action name itself will be used.
        prerequisites: An action-specific `struct` whose fields can be accessed
            by the action configurators to add files and other dependent data to
            the command line.
        swift_toolchain: The Swift toolchain being used to build.
        **kwargs: Additional arguments passed directly to `actions.run`.
    """
    tool_config = swift_toolchain.tool_configs.get(action_name)
    if not tool_config:
        fail(
            "There is no tool configured for the action " +
            "'{}' in this toolchain. If this action is ".format(action_name) +
            "supported conditionally, you must call 'is_action_enabled' " +
            "before attempting to register it.",
        )

    args = actions.args()
    if tool_config.use_param_file:
        args.set_param_file_format("multiline")
        args.use_param_file("@%s", use_always = True)

    execution_requirements = dict(tool_config.execution_requirements)

    # If the tool configuration says to use the worker process, then use the
    # worker as the actual executable and pass the tool as the first argument
    # (and as a tool input). We do this in a separate `Args` object so that the
    # tool name/path is added directly to the command line, not added to a param
    # file.
    #
    # If the tool configuration says not to use the worker, then we just use the
    # tool as the executable directly.
    tools = []
    tool_executable_args = actions.args()
    if tool_config.worker_mode:
        # Only enable persistent workers if the toolchain supports response
        # files, because the worker unconditionally writes its arguments into
        # one to prevent command line overflow in this mode.
        if (
            tool_config.worker_mode == "persistent" and
            tool_config.use_param_file
        ):
            execution_requirements["supports-workers"] = "1"
            execution_requirements["requires-worker-protocol"] = "json"

        executable = swift_toolchain.swift_worker
        tool_executable_args.add(tool_config.executable)
        if not types.is_string(tool_config.executable):
            tools.append(tool_config.executable)
    else:
        executable = tool_config.executable
    tools.extend(tool_config.additional_tools)

    # If the tool configuration has any required arguments, add those first.
    if tool_config.args:
        args.add_all(tool_config.args)

    # Apply the action configs that are relevant based on the requested action
    # and feature configuration, to populate the `Args` object and collect the
    # required inputs.
    action_inputs = _apply_action_configs(
        action_name = action_name,
        args = args,
        feature_configuration = feature_configuration,
        prerequisites = prerequisites,
        swift_toolchain = swift_toolchain,
    )

    actions.run(
        arguments = [tool_executable_args, args],
        env = tool_config.env,
        exec_group = exec_group,
        executable = executable,
        execution_requirements = execution_requirements,
        inputs = depset(
            action_inputs.inputs,
            transitive = action_inputs.transitive_inputs,
        ),
        mnemonic = mnemonic if mnemonic else action_name,
        resource_set = tool_config.resource_set,
        tools = depset(
            tools,
            transitive = action_inputs.additional_tools,
        ),
        use_default_shell_env = USE_DEFAULT_SHELL_ENV,
        **kwargs
    )
