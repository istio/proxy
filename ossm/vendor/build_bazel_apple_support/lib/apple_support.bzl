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
"""Definitions for registering actions on Apple platforms."""

load("@bazel_skylib//lib:types.bzl", "types")

# Options to declare the level of Xcode path resolving needed in an `apple_support.run()`
# invocation.
_XCODE_PATH_RESOLVE_LEVEL = struct(
    none = None,
    args = "args",
    args_and_files = "args_and_files",
)

# This is a proxy for being on bazel 7.x which has
# --incompatible_merge_fixed_and_default_shell_env enabled by default
_USE_DEFAULT_SHELL_ENV = not hasattr(apple_common, "apple_crosstool_transition")

_XCODE_PROCESSOR__ARGS = r"""#!/bin/bash

set -eu

# SYNOPSIS
#   Rewrites any Bazel placeholder strings in the given argument string,
#   echoing the result.
#
# USAGE
#   rewrite_argument <argument>
function rewrite_argument {
  ARG="$1"
  ARG="${ARG//__BAZEL_XCODE_DEVELOPER_DIR__/$DEVELOPER_DIR}"
  ARG="${ARG//__BAZEL_XCODE_SDKROOT__/$SDKROOT}"
  echo "$ARG"
}

TOOLNAME="$1"
shift

ARGS=()

for ARG in "$@" ; do
  ARGS+=("$(rewrite_argument "$ARG")")
done

exec "$TOOLNAME" "${ARGS[@]}"
"""

_XCODE_PROCESSOR__ARGS_AND_FILES = r"""#!/bin/bash

set -eu

# SYNOPSIS
#   Rewrites any Bazel placeholder strings in the given argument string,
#   echoing the result.
#
# USAGE
#   rewrite_argument <argument>
function rewrite_argument {
  ARG="$1"
  ARG="${ARG//__BAZEL_XCODE_DEVELOPER_DIR__/$DEVELOPER_DIR}"
  ARG="${ARG//__BAZEL_XCODE_SDKROOT__/$SDKROOT}"
  echo "$ARG"
}

# SYNOPSIS
#   Rewrites any Bazel placeholder strings in the given params file, if any.
#   If there were no substitutions to be made, the original path is echoed back
#   out; otherwise, this function echoes the path to a temporary file
#   containing the rewritten file.
#
# USAGE
#   rewrite_params_file <path>
function rewrite_params_file {
  PARAMSFILE="$1"
  if grep -qe '__BAZEL_XCODE_\(DEVELOPER_DIR\|SDKROOT\)__' "$PARAMSFILE" ; then
    NEWFILE="$(mktemp "${TMPDIR%/}/bazel_xcode_wrapper_params.XXXXXXXXXX")"
    sed \
        -e "s#__BAZEL_XCODE_DEVELOPER_DIR__#$DEVELOPER_DIR#g" \
        -e "s#__BAZEL_XCODE_SDKROOT__#$SDKROOT#g" \
        "$PARAMSFILE" > "$NEWFILE"
    echo "$NEWFILE"
  else
    # There were no placeholders to substitute, so just return the original
    # file.
    echo "$PARAMSFILE"
  fi
}

TOOLNAME="$1"
shift

ARGS=()

# If any temporary files are created (like rewritten response files), clean
# them up when the script exits.
TEMPFILES=()
trap '[[ ${#TEMPFILES[@]} -ne 0 ]] && rm "${TEMPFILES[@]}"' EXIT

for ARG in "$@" ; do
  case "$ARG" in
  @*)
    PARAMSFILE="${ARG:1}"
    NEWFILE=$(rewrite_params_file "$PARAMSFILE")
    if [[ "$PARAMSFILE" != "$NEWFILE" ]] ; then
      TEMPFILES+=("$NEWFILE")
    fi
    ARG="@$NEWFILE"
    ;;
  *)
    ARG=$(rewrite_argument "$ARG")
    ;;
  esac
  ARGS+=("$ARG")
done

# We can't use `exec` here because we need to make sure the `trap` runs
# afterward.
"$TOOLNAME" "${ARGS[@]}"
"""

def _validate_ctx_xor_platform_requirements(*, ctx, actions, apple_fragment, xcode_config):
    """Raises an error if there is overlap in platform requirements or if they are insufficent."""

    if ctx != None and any([actions, xcode_config, apple_fragment]):
        fail("Can't specific ctx along with actions, xcode_config, apple_fragment.")
    if ctx == None and not all([actions, xcode_config, apple_fragment]):
        fail("Must specify all of actions, xcode_config, and apple_fragment.")
    if ctx != None:
        _validate_ctx_attribute_present(ctx, "_xcode_config")

def _platform_frameworks_path_placeholder(*, apple_fragment):
    """Returns the platform's frameworks directory, anchored to the Xcode path placeholder.

    Args:
        apple_fragment: A reference to the apple fragment. Typically from `ctx.fragments.apple`.

    Returns:
        Returns a string with the platform's frameworks directory, anchored to the Xcode path
        placeholder.
    """
    return "{xcode_path}/Platforms/{platform_name}.platform/Developer/Library/Frameworks".format(
        platform_name = apple_fragment.single_arch_platform.name_in_plist,
        xcode_path = _xcode_path_placeholder(),
    )

def _sdkroot_path_placeholder():
    """Returns a placeholder value to be replaced with SDKROOT during action execution.

    In order to get this values replaced, you'll need to use the `apple_support.run()` API by
    setting the `xcode_path_resolve_level` argument to either the
    `apple_support.xcode_path_resolve_level.args` or
    `apple_support.xcode_path_resolve_level.args_and_files` value.

    Returns:
        Returns a placeholder value to be replaced with SDKROOT during action execution.
    """
    return "__BAZEL_XCODE_SDKROOT__"

def _xcode_path_placeholder():
    """Returns a placeholder value to be replaced with DEVELOPER_DIR during action execution.

    In order to get this values replaced, you'll need to use the `apple_support.run()` API by
    setting the `xcode_path_resolve_level` argument to either the
    `apple_support.xcode_path_resolve_level.args` or
    `apple_support.xcode_path_resolve_level.args_and_files` value.

    Returns:
        Returns a placeholder value to be replaced with DEVELOPER_DIR during action execution.
    """
    return "__BAZEL_XCODE_DEVELOPER_DIR__"

def _kwargs_for_apple_platform(
        *,
        apple_fragment,
        xcode_config,
        **kwargs):
    """Returns a modified dictionary with required arguments to run on Apple platforms."""
    processed_args = dict(kwargs)

    merged_env = {}
    original_env = processed_args.get("env")
    if original_env:
        merged_env.update(original_env)

    if "use_default_shell_env" not in processed_args:
        processed_args["use_default_shell_env"] = _USE_DEFAULT_SHELL_ENV

    # Add the environment variables required for DEVELOPER_DIR and SDKROOT last to avoid clients
    # overriding these values.
    merged_env.update(apple_common.apple_host_system_env(xcode_config))
    merged_env.update(
        apple_common.target_apple_env(xcode_config, apple_fragment.single_arch_platform),
    )

    merged_execution_requirements = {}
    original_execution_requirements = processed_args.get("execution_requirements")
    if original_execution_requirements:
        merged_execution_requirements.update(original_execution_requirements)

    # Add the Xcode execution requirements last to avoid clients overriding these values.
    merged_execution_requirements.update(xcode_config.execution_info())

    processed_args["env"] = merged_env
    processed_args["execution_requirements"] = merged_execution_requirements
    return processed_args

def _validate_ctx_attribute_present(ctx, attribute_name):
    """Validates that the given attribute is present for the rule, failing otherwise."""
    if not hasattr(ctx.attr, attribute_name):
        fail("\n".join([
            "",
            "ERROR: This rule requires the '{}' attribute to be present. ".format(attribute_name),
            "To add this attribute, modify your rule definition like this:",
            "",
            "load(\"@bazel_skylib//lib:dicts.bzl\", \"dicts\")",
            "load(",
            "    \"@build_bazel_apple_support//lib:apple_support.bzl\",",
            "    \"apple_support\",",
            ")",
            "",
            "your_rule_name = rule(",
            "    attrs = dicts.add(apple_support.action_required_attrs(), {",
            "        # other attributes",
            "    }),",
            "    # other rule arguments",
            ")",
            "",
        ]))

def _action_required_attrs():
    """Returns a dictionary with required attributes for registering actions on Apple platforms.

    This method adds private attributes which should not be used outside of the apple_support
    codebase. It also adds the following attributes which are considered to be public for rule
    maintainers to use:

     * `_xcode_config`: Attribute that references a target containing the single
       `apple_common.XcodeVersionConfig` provider. This provider can be used to inspect Xcode-related
       properties about the Xcode being used for the build, as specified with the `--xcode_version`
       Bazel flag. The most common way to retrieve this provider is:
       `ctx.attr._xcode_config[apple_common.XcodeVersionConfig]`.

    The returned `dict` can be added to the rule's attributes using Skylib's `dicts.add()` method.

    Returns:
        A `dict` object containing attributes to be added to rule implementations.
    """
    return {
        "_xcode_config": attr.label(
            default = configuration_field(
                name = "xcode_config_label",
                fragment = "apple",
            ),
        ),
    }

def _platform_constraint_attrs():
    """Returns a dictionary of all known Apple platform constraints that can be resolved.

    The returned `dict` can be added to the rule's attributes using Skylib's `dicts.add()` method.

    Returns:
        A `dict` object containing attributes to be added to rule implementations.
    """
    return {
        "_ios_constraint": attr.label(
            default = Label("@platforms//os:ios"),
        ),
        "_macos_constraint": attr.label(
            default = Label("@platforms//os:macos"),
        ),
        "_tvos_constraint": attr.label(
            default = Label("@platforms//os:tvos"),
        ),
        "_visionos_constraint": attr.label(
            default = Label("@platforms//os:visionos"),
        ),
        "_watchos_constraint": attr.label(
            default = Label("@platforms//os:watchos"),
        ),
        "_arm64_constraint": attr.label(
            default = Label("@platforms//cpu:arm64"),
        ),
        "_arm64e_constraint": attr.label(
            default = Label("@platforms//cpu:arm64e"),
        ),
        "_arm64_32_constraint": attr.label(
            default = Label("@platforms//cpu:arm64_32"),
        ),
        "_armv7k_constraint": attr.label(
            default = Label("@platforms//cpu:armv7k"),
        ),
        "_x86_64_constraint": attr.label(
            default = Label("@platforms//cpu:x86_64"),
        ),
        "_apple_device_constraint": attr.label(
            default = Label("//constraints:device"),
        ),
        "_apple_simulator_constraint": attr.label(
            default = Label("//constraints:simulator"),
        ),
    }

def _run(
        ctx = None,
        xcode_path_resolve_level = _XCODE_PATH_RESOLVE_LEVEL.none,
        *,
        actions = None,
        xcode_config = None,
        apple_fragment = None,
        **kwargs):
    """Registers an action to run on an Apple machine.

    In order to use `apple_support.run()`, you'll need to modify your rule definition to add the
    following:

      * `fragments = ["apple"]`
      * Add the `apple_support.action_required_attrs()` attributes to the `attrs` dictionary. This
        can be done using the `dicts.add()` method from Skylib.

    This method registers an action to run on an Apple machine, configuring it to ensure that the
    `DEVELOPER_DIR` and `SDKROOT` environment variables are set.

    If the `xcode_path_resolve_level` is enabled, this method will replace the given `executable`
    with a wrapper script that will replace all instances of the `__BAZEL_XCODE_DEVELOPER_DIR__` and
    `__BAZEL_XCODE_SDKROOT__` placeholders in the given arguments with the values of `DEVELOPER_DIR`
    and `SDKROOT`, respectively.

    In your rule implementation, you can use references to Xcode through the
    `apple_support.path_placeholders` API, which in turn uses the placeholder values as described
    above. The available APIs are:

      * `apple_support.path_placeholders.xcode()`: Returns a reference to the Xcode.app
        installation path.
      * `apple_support.path_placeholders.sdkroot()`: Returns a reference to the SDK root path.
      * `apple_support.path_placeholders.platform_frameworks(ctx)`: Returns the Frameworks path
        within the Xcode installation, for the requested platform.

    If the `xcode_path_resolve_level` value is:

      * `apple_support.xcode_path_resolve_level.none`: No processing will be done to the given
        `arguments`.
      * `apple_support.xcode_path_resolve_level.args`: Only instances of the placeholders in the
         argument strings will be replaced.
      * `apple_support.xcode_path_resolve_level.args_and_files`: Instances of the placeholders in
         the arguments strings and instances of the placeholders within response files (i.e. any
         path argument beginning with `@`) will be replaced.

    Args:
        ctx: The context of the rule registering the action. Deprecated.
        xcode_path_resolve_level: The level of Xcode path replacement required for the action.
        actions: The actions provider from ctx.actions. Required if ctx is not given.
        xcode_config: The xcode_config as found in the current rule or aspect's
            context. Typically from `ctx.attr._xcode_config[apple_common.XcodeVersionConfig]`.
            Required if ctx is not given.
        apple_fragment: A reference to the apple fragment. Typically from `ctx.fragments.apple`.
            Required if ctx is not given.
        **kwargs: See `ctx.actions.run` for the rest of the available arguments.
    """
    _validate_ctx_xor_platform_requirements(
        ctx = ctx,
        actions = actions,
        apple_fragment = apple_fragment,
        xcode_config = xcode_config,
    )

    if not actions:
        actions = ctx.actions
    if not xcode_config:
        xcode_config = ctx.attr._xcode_config[apple_common.XcodeVersionConfig]
    if not apple_fragment:
        apple_fragment = ctx.fragments.apple

    if xcode_path_resolve_level == _XCODE_PATH_RESOLVE_LEVEL.none:
        actions.run(**_kwargs_for_apple_platform(
            xcode_config = xcode_config,
            apple_fragment = apple_fragment,
            **kwargs
        ))
        return

    # Since a label/name isn't passed in, use the first output to derive a name
    # that will hopefully be unique.
    output0 = kwargs.get("outputs")[0]
    if xcode_path_resolve_level == _XCODE_PATH_RESOLVE_LEVEL.args:
        script = _XCODE_PROCESSOR__ARGS
        suffix = "args"
    else:
        script = _XCODE_PROCESSOR__ARGS_AND_FILES
        suffix = "args_and_files"
    processor_script = actions.declare_file("{}_{}_processor_script_{}.sh".format(
        output0.basename,
        hash(output0.short_path),
        suffix,
    ))
    actions.write(processor_script, script, is_executable = True)

    processed_kwargs = _kwargs_for_apple_platform(
        xcode_config = xcode_config,
        apple_fragment = apple_fragment,
        **kwargs
    )

    all_arguments = []

    # If the client requires Xcode path resolving, push the original executable to be the first
    # argument, as the executable will be set to be the xcode_path_wrapper script.
    # Note: Bounce through an actions.args() incase the executable was a `File`, this allows it
    # to then work within the arguments list.
    executable_args = actions.args()
    original_executable = processed_kwargs.pop("executable")

    # actions.run supports multiple executable types. (File; or string; or FilesToRunProvider)
    # If the passed in executable is a FilesToRunProvider, only add the main executable to the
    # should be added to the executable args.
    if type(original_executable) == "FilesToRunProvider":
        executable_args.add(original_executable.executable)
    else:
        executable_args.add(original_executable)
    all_arguments.append(executable_args)

    # Append the original arguments to the full list of arguments, after the original executable.
    original_args_list = processed_kwargs.pop("arguments", [])
    if not original_args_list:
        fail("Error: Does not make sense to request args processing without any `arguments`.")
    all_arguments.extend(original_args_list)

    # We also need to include the user executable in the "tools" argument of the action, since it
    # won't be referenced by "executable" anymore.
    original_tools = processed_kwargs.pop("tools", None)
    if types.is_list(original_tools):
        all_tools = [original_executable] + original_tools
    elif types.is_depset(original_tools):
        all_tools = depset([original_executable], transitive = [original_tools])
    elif original_tools:
        fail("'tools' argument must be a sequence or depset.")
    elif not types.is_string(original_executable):
        # Only add the user_executable to the "tools" list if it's a File, not a string.
        all_tools = [original_executable]
    else:
        all_tools = []

    actions.run(
        executable = processor_script,
        arguments = all_arguments,
        tools = all_tools,
        **processed_kwargs
    )

def _run_shell(
        ctx = None,
        *,
        actions = None,
        xcode_config = None,
        apple_fragment = None,
        **kwargs):
    """Registers a shell action to run on an Apple machine.

    In order to use `apple_support.run_shell()`, you'll need to modify your rule definition to add
    the following:

      * `fragments = ["apple"]`
      * Add the `apple_support.action_required_attrs()` attributes to the `attrs` dictionary. This
        can be done using the `dicts.add()` method from Skylib.

    This method registers an action to run on an Apple machine, configuring it to ensure that the
    `DEVELOPER_DIR` and `SDKROOT` environment variables are set.

    `run_shell` does not support placeholder substitution. To achieve placeholder substitution,
    please use `run` instead.

    Args:
        ctx: The context of the rule registering the action. Deprecated.
        actions: The actions provider from ctx.actions.
        xcode_config: The xcode_config as found in the current rule or aspect's
            context. Typically from `ctx.attr._xcode_config[apple_common.XcodeVersionConfig]`.
            Required if ctx is not given.
        apple_fragment: A reference to the apple fragment. Typically from `ctx.fragments.apple`.
            Required if ctx is not given.
        **kwargs: See `ctx.actions.run_shell` for the rest of the available arguments.
    """
    _validate_ctx_xor_platform_requirements(
        ctx = ctx,
        actions = actions,
        apple_fragment = apple_fragment,
        xcode_config = xcode_config,
    )

    if not actions:
        actions = ctx.actions
    if not xcode_config:
        xcode_config = ctx.attr._xcode_config[apple_common.XcodeVersionConfig]
    if not apple_fragment:
        apple_fragment = ctx.fragments.apple

    actions.run_shell(**_kwargs_for_apple_platform(
        xcode_config = xcode_config,
        apple_fragment = apple_fragment,
        **kwargs
    ))

def _target_arch_from_rule_ctx(ctx):
    """Returns a `String` representing the target architecture based on constraints.

    The returned `String` will represent a cpu architecture, such as `arm64` or `arm64e`.

    In order to use `apple_support.target_arch_from_rule_ctx()`, you'll need to modify your rule
    definition to add the following:

      * Add the `apple_support.platform_constraint_attrs()` attributes to the `attrs` dictionary.
        This can be done using the `dicts.add()` method from Skylib.

    Args:
        ctx: The context of the rule that has Apple platform constraint attributes.

    Returns:
        A `String` representing the selected target architecture or cpu type (e.g. `arm64`,
        `arm64e`).
    """
    arm64_constraint = ctx.attr._arm64_constraint[platform_common.ConstraintValueInfo]
    arm64e_constraint = ctx.attr._arm64e_constraint[platform_common.ConstraintValueInfo]
    arm64_32_constraint = ctx.attr._arm64_32_constraint[platform_common.ConstraintValueInfo]
    armv7k_constraint = ctx.attr._armv7k_constraint[platform_common.ConstraintValueInfo]
    x86_64_constraint = ctx.attr._x86_64_constraint[platform_common.ConstraintValueInfo]

    if ctx.target_platform_has_constraint(arm64_constraint):
        return "arm64"
    elif ctx.target_platform_has_constraint(arm64e_constraint):
        return "arm64e"
    elif ctx.target_platform_has_constraint(arm64_32_constraint):
        return "arm64_32"
    elif ctx.target_platform_has_constraint(armv7k_constraint):
        return "armv7k"
    elif ctx.target_platform_has_constraint(x86_64_constraint):
        return "x86_64"
    fail("ERROR: A valid Apple cpu constraint could not be found from the resolved toolchain.")

def _target_environment_from_rule_ctx(ctx):
    """Returns a `String` representing the target environment based on constraints.

    The returned `String` will represent an environment, such as `device` or `simulator`.

    For consistency with other Apple platforms, `macos` is considered to be a `device`.

    In order to use `apple_support.target_environment_from_rule_ctx()`, you'll need to modify your
    rule definition to add the following:

      * Add the `apple_support.platform_constraint_attrs()` attributes to the `attrs` dictionary.
        This can be done using the `dicts.add()` method from Skylib.

    Args:
        ctx: The context of the rule that has Apple platform constraint attributes.

    Returns:
        A `String` representing the selected environment (e.g. `device`, `simulator`).
    """
    device_constraint = ctx.attr._apple_device_constraint[platform_common.ConstraintValueInfo]
    simulator_constraint = ctx.attr._apple_simulator_constraint[platform_common.ConstraintValueInfo]

    if ctx.target_platform_has_constraint(device_constraint):
        return "device"
    elif ctx.target_platform_has_constraint(simulator_constraint):
        return "simulator"
    fail("ERROR: A valid Apple environment (device, simulator) constraint could not be found from" +
         " the resolved toolchain.")

def _target_os_from_rule_ctx(ctx):
    """Returns a `String` representing the target OS based on constraints.

    The returned `String` will match an equivalent value from one of the platform definitions in
    `apple_common.platform_type`, such as `ios` or `macos`.

    In order to use `apple_support.target_os_from_rule_ctx()`, you'll need to modify your rule
    definition to add the following:

      * Add the `apple_support.platform_constraint_attrs()` attributes to the `attrs` dictionary.
        This can be done using the `dicts.add()` method from Skylib.

    Args:
        ctx: The context of the rule that has Apple platform constraint attributes.

    Returns:
        A `String` representing the selected Apple OS.
    """
    ios_constraint = ctx.attr._ios_constraint[platform_common.ConstraintValueInfo]
    macos_constraint = ctx.attr._macos_constraint[platform_common.ConstraintValueInfo]
    tvos_constraint = ctx.attr._tvos_constraint[platform_common.ConstraintValueInfo]
    visionos_constraint = ctx.attr._visionos_constraint[platform_common.ConstraintValueInfo]
    watchos_constraint = ctx.attr._watchos_constraint[platform_common.ConstraintValueInfo]

    if ctx.target_platform_has_constraint(ios_constraint):
        return str(apple_common.platform_type.ios)
    elif ctx.target_platform_has_constraint(macos_constraint):
        return str(apple_common.platform_type.macos)
    elif ctx.target_platform_has_constraint(tvos_constraint):
        return str(apple_common.platform_type.tvos)
    elif ctx.target_platform_has_constraint(visionos_constraint):
        return str(apple_common.platform_type.visionos)
    elif ctx.target_platform_has_constraint(watchos_constraint):
        return str(apple_common.platform_type.watchos)
    fail("ERROR: A valid Apple platform constraint could not be found from the resolved toolchain.")

apple_support = struct(
    action_required_attrs = _action_required_attrs,
    path_placeholders = struct(
        platform_frameworks = _platform_frameworks_path_placeholder,
        sdkroot = _sdkroot_path_placeholder,
        xcode = _xcode_path_placeholder,
    ),
    platform_constraint_attrs = _platform_constraint_attrs,
    run = _run,
    run_shell = _run_shell,
    target_arch_from_rule_ctx = _target_arch_from_rule_ctx,
    target_environment_from_rule_ctx = _target_environment_from_rule_ctx,
    target_os_from_rule_ctx = _target_os_from_rule_ctx,
    xcode_path_resolve_level = _XCODE_PATH_RESOLVE_LEVEL,
)
