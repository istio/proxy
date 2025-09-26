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
"""Rules for accessing cc build variables in bazel toolchains safely."""

load("//cc/toolchains:cc_toolchain_info.bzl", "ActionTypeSetInfo", "BuiltinVariablesInfo", "VariableInfo")
load(":collect.bzl", "collect_action_types", "collect_provider")

visibility([
    "//cc/toolchains/variables",
    "//tests/rule_based_toolchain/...",
])

types = struct(
    unknown = dict(name = "unknown", repr = "unknown"),
    void = dict(name = "void", repr = "void"),
    string = dict(name = "string", repr = "string"),
    bool = dict(name = "bool", repr = "bool"),
    # File and directory are basically the same thing as string for now.
    file = dict(name = "file", repr = "File"),
    directory = dict(name = "directory", repr = "directory"),
    option = lambda element: dict(
        name = "option",
        elements = element,
        repr = "Option[%s]" % element["repr"],
    ),
    list = lambda elements: dict(
        name = "list",
        elements = elements,
        repr = "List[%s]" % elements["repr"],
    ),
    struct = lambda **kv: dict(
        name = "struct",
        kv = kv,
        repr = "struct(%s)" % ", ".join([
            "{k}={v}".format(k = k, v = v["repr"])
            for k, v in sorted(kv.items())
        ]),
    ),
)

def _cc_variable_impl(ctx):
    return [VariableInfo(
        name = ctx.label.name,
        label = ctx.label,
        type = json.decode(ctx.attr.type),
        actions = collect_action_types(ctx.attr.actions) if ctx.attr.actions else None,
    )]

_cc_variable = rule(
    implementation = _cc_variable_impl,
    attrs = {
        "actions": attr.label_list(providers = [ActionTypeSetInfo]),
        "type": attr.string(mandatory = True),
    },
    provides = [VariableInfo],
)

def cc_variable(name, type, **kwargs):
    """Exposes a toolchain variable to use in toolchain argument expansions.

    This internal rule exposes [toolchain variables](https://bazel.build/docs/cc-toolchain-config-reference#cctoolchainconfiginfo-build-variables)
    that may be expanded in `cc_args` or `cc_nested_args`
    rules. Because these varaibles merely expose variables inherrent to Bazel,
    it's not possible to declare custom variables.

    For a full list of available variables, see
    [@rules_cc//cc/toolchains/varaibles:BUILD](https://github.com/bazelbuild/rules_cc/tree/main/cc/toolchains/variables/BUILD).

    Example:
    ```
    load("//cc/toolchains/impl:variables.bzl", "cc_variable")

    # Defines two targets, ":foo" and ":foo.bar"
    cc_variable(
        name = "foo",
        type = types.list(types.struct(bar = types.string)),
    )
    ```

    Args:
        name: (str) The name of the outer variable, and the rule.
        type: The type of the variable, constructed using `types` factory in
            [@rules_cc//cc/toolchains/impl:variables.bzl](https://github.com/bazelbuild/rules_cc/tree/main/cc/toolchains/impl/variables.bzl).
        **kwargs: [common attributes](https://bazel.build/reference/be/common-definitions#common-attributes) that should be applied to this rule.
    """
    _cc_variable(name = name, type = json.encode(type), **kwargs)

def _cc_builtin_variables_impl(ctx):
    return [BuiltinVariablesInfo(variables = {
        variable.name: variable
        for variable in collect_provider(ctx.attr.srcs, VariableInfo)
    })]

cc_builtin_variables = rule(
    implementation = _cc_builtin_variables_impl,
    attrs = {
        "srcs": attr.label_list(providers = [VariableInfo]),
    },
)

def get_type(*, name, variables, overrides, actions, args_label, nested_label, fail):
    """Gets the type of a variable.

    Args:
        name: (str) The variable to look up.
        variables: (dict[str, VariableInfo]) Mapping from variable name to
          metadata. Top-level variables only
        overrides: (dict[str, type]) Mapping from variable names to type.
          Can be used for nested variables.
        actions: (depset[ActionTypeInfo]) The set of actions for which the
          variable is requested.
        args_label: (Label) The label for the args that included the rule that
          references this variable. Only used for error messages.
        nested_label: (Label) The label for the rule that references this
          variable. Only used for error messages.
        fail: A function to be called upon failure. Use for testing only.
    Returns:
        The type of the variable "name".
    """
    outer = name.split(".")[0]
    if outer not in variables:
        # With a fail function, we actually need to return since the fail
        # function doesn't propagate.
        fail("The variable %s does not exist. Did you mean one of the following?\n%s" % (outer, "\n".join(sorted(variables))))

        # buildifier: disable=unreachable
        return types.void

    if variables[outer].actions != None:
        valid_actions = variables[outer].actions.to_list()
        for action in actions:
            if action not in valid_actions:
                fail("The variable {var} is inaccessible from the action {action}. This is required because it is referenced in {nested_label}, which is included by {args_label}, which references that action".format(
                    var = variables[outer].label,
                    nested_label = nested_label,
                    args_label = args_label,
                    action = action.label,
                ))

                # buildifier: disable=unreachable
                return types.void

    type = overrides.get(outer, variables[outer].type)

    parent = outer
    for part in name.split(".")[1:]:
        full = parent + "." + part

        if type["name"] != "struct":
            extra_error = ""
            if type["name"] == "list" and type["elements"]["name"] == "struct":
                extra_error = " Maybe you meant to use iterate_over."

            fail("Attempted to access %r, but %r was not a struct - it had type %s.%s" % (full, parent, type["repr"], extra_error))

            # buildifier: disable=unreachable
            return types.void

        if part not in type["kv"] and full not in overrides:
            attrs = []
            for attr, value in sorted(type["kv"].items()):
                attrs.append("%s: %s" % (attr, value["repr"]))
            fail("Unable to find %r in %r, which had the following attributes:\n%s" % (part, parent, "\n".join(attrs)))

            # buildifier: disable=unreachable
            return types.void

        type = overrides.get(full, type["kv"][part])
        parent = full

    return type
