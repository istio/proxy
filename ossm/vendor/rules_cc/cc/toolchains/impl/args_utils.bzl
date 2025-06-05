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
"""Helper functions for working with args."""

load(":variables.bzl", "get_type")

visibility([
    "//cc/toolchains",
    "//tests/rule_based_toolchain/...",
])

def get_action_type(args_list, action_type):
    """Returns the corresponding entry in ArgsListInfo.by_action.

    Args:
        args_list: (ArgsListInfo) The args list to look through
        action_type: (ActionTypeInfo) The action type to look up.
    Returns:
        The information corresponding to this action type.

    """
    for args in args_list.by_action:
        if args.action == action_type:
            return args

    return struct(action = action_type, args = tuple(), files = depset([]))

def validate_nested_args(*, nested_args, variables, actions, label, fail = fail):
    """Validates the typing for an nested_args invocation.

    Args:
        nested_args: (NestedArgsInfo) The nested_args to validate
        variables: (Dict[str, VariableInfo]) A mapping from variable name to
          the metadata (variable type and valid actions).
        actions: (List[ActionTypeInfo]) The actions we require these variables
          to be valid for.
        label: (Label) The label of the rule we're currently validating.
          Used for error messages.
        fail: The fail function. Use for testing only.
    """
    stack = [(nested_args, {})]

    for _ in range(9999999):
        if not stack:
            break
        nested_args, overrides = stack.pop()
        if nested_args.iterate_over != None or nested_args.unwrap_options:
            # Make sure we don't keep using the same object.
            overrides = dict(**overrides)

        if nested_args.iterate_over != None:
            type = get_type(
                name = nested_args.iterate_over,
                variables = variables,
                overrides = overrides,
                actions = actions,
                args_label = label,
                nested_label = nested_args.label,
                fail = fail,
            )
            if type["name"] == "list":
                # Rewrite the type of the thing we iterate over from a List[T]
                # to a T.
                overrides[nested_args.iterate_over] = type["elements"]
            elif type["name"] == "option" and type["elements"]["name"] == "list":
                # Rewritten below after requires_not_none is checked
                pass
            else:
                fail("Attempting to iterate over %s, but it was not a list - it was a %s" % (nested_args.iterate_over, type["repr"]))

        # 1) Validate variables marked with after_option_unwrap = False.
        # 2) Unwrap Option[T] to T as required.
        # 3) Validate variables marked with after_option_unwrap = True.
        for after_option_unwrap in [False, True]:
            for var_name, requirements in nested_args.requires_types.items():
                for requirement in requirements:
                    if requirement.after_option_unwrap == after_option_unwrap:
                        type = get_type(
                            name = var_name,
                            variables = variables,
                            overrides = overrides,
                            actions = actions,
                            args_label = label,
                            nested_label = nested_args.label,
                            fail = fail,
                        )
                        if type["name"] not in requirement.valid_types:
                            fail("{msg}, but {var_name} has type {type}".format(
                                var_name = var_name,
                                msg = requirement.msg,
                                type = type["repr"],
                            ))

            # Only unwrap the options after the first iteration of this loop.
            if not after_option_unwrap:
                for var in nested_args.unwrap_options:
                    type = get_type(
                        name = var,
                        variables = variables,
                        overrides = overrides,
                        actions = actions,
                        args_label = label,
                        nested_label = nested_args.label,
                        fail = fail,
                    )
                    if nested_args.iterate_over and type["name"] == "option" and type["elements"]["name"] == "list":
                        # Rewrite Option[List[T]] to T.
                        overrides[nested_args.iterate_over] = type["elements"]["elements"]
                    elif type["name"] == "option":
                        overrides[var] = type["elements"]

        for child in nested_args.nested:
            stack.append((child, overrides))
