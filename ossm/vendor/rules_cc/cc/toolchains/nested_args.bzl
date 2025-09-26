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

load(
    "//cc/toolchains/impl:nested_args.bzl",
    "NESTED_ARGS_ATTRS",
    "nested_args_provider_from_ctx",
)
load(
    ":cc_toolchain_info.bzl",
    "NestedArgsInfo",
)

visibility("public")

_cc_nested_args = rule(
    implementation = lambda ctx: [nested_args_provider_from_ctx(ctx)],
    attrs = NESTED_ARGS_ATTRS,
    provides = [NestedArgsInfo],
    doc = """Declares a list of arguments bound to a set of actions.

Roughly equivalent to ctx.actions.args()

Examples:
    cc_nested_args(
        name = "warnings_as_errors",
        args = ["-Werror"],
    )
""",
)

def cc_nested_args(
        *,
        name,
        args = None,
        data = None,
        format = {},
        iterate_over = None,
        nested = None,
        requires_not_none = None,
        requires_none = None,
        requires_true = None,
        requires_false = None,
        requires_equal = None,
        requires_equal_value = None,
        **kwargs):
    """Nested arguments for use in more complex `cc_args` expansions.

    While this rule is very similar in shape to `cc_args`, it is intended to be used as a
    dependency of `cc_args` to provide additional arguments that should be applied to the
    same actions as defined by the parent `cc_args` rule. The key motivation for this rule
    is to allow for more complex variable-based argument expensions.

    Prefer expressing collections of arguments as `cc_args` and
    `cc_args_list` rules when possible.

    For living examples of how this rule is used, see the usages here:
        https://github.com/bazelbuild/rules_cc/tree/main/cc/toolchains/args/runtime_library_search_directories/BUILD
        https://github.com/bazelbuild/rules_cc/tree/main/cc/toolchains/args/libraries_to_link/BUILD

    Note: These examples are non-trivial, but they illustrate when it is absolutely necessary to
    use this rule.

    Args:
        name: (str) The name of the target.
        args: (List[str]) The command-line arguments that are applied by using this rule. This is
            mutually exclusive with [nested](#cc_nested_args-nested).
        data: (List[Label]) A list of runtime data dependencies that are required for these
            arguments to work as intended.
        format: (Dict[str, Label]) A mapping of format strings to the label of the corresponding
            `cc_variable` that the value should be pulled from. All instances of
            `{variable_name}` will be replaced with the expanded value of `variable_name` in this
            dictionary. The complete list of possible variables can be found in
            https://github.com/bazelbuild/rules_cc/tree/main/cc/toolchains/variables/BUILD.
            It is not possible to declare custom variables--these are inherent to Bazel itself.
        iterate_over: (Label) The label of a `cc_variable` that should be iterated
            over. This is intended for use with built-in variables that are lists.
        nested: (List[Label]) A list of `cc_nested_args` rules that should be
            expanded to command-line arguments when this rule is used. This is mutually exclusive
            with [args](#cc_nested_args-args).
        requires_not_none: (Label) The label of a `cc_variable` that should be checked
            for existence before expanding this rule. If the variable is None, this rule will be
            ignored.
        requires_none: (Label) The label of a `cc_variable` that should be checked for
            non-existence before expanding this rule. If the variable is not None, this rule will be
            ignored.
        requires_true: (Label) The label of a `cc_variable` that should be checked for
            truthiness before expanding this rule. If the variable is false, this rule will be
            ignored.
        requires_false: (Label) The label of a `cc_variable` that should be checked
            for falsiness before expanding this rule. If the variable is true, this rule will be
            ignored.
        requires_equal: (Label) The label of a `cc_variable` that should be checked
            for equality before expanding this rule. If the variable is not equal to
            (requires_equal_value)[#cc_nested_args-requires_equal_value], this rule will be ignored.
        requires_equal_value: (str) The value to compare
            (requires_equal)[#cc_nested_args-requires_equal] against.
        **kwargs: [common attributes](https://bazel.build/reference/be/common-definitions#common-attributes) that should be applied to this rule.
    """
    return _cc_nested_args(
        name = name,
        args = args,
        data = data,
        # We flip the key/value pairs in the dictionary here because Bazel doesn't have a
        # string-keyed label dict attribute type.
        format = {k: v for v, k in format.items()},
        iterate_over = iterate_over,
        nested = nested,
        requires_not_none = requires_not_none,
        requires_none = requires_none,
        requires_true = requires_true,
        requires_false = requires_false,
        requires_equal = requires_equal,
        requires_equal_value = requires_equal_value,
        **kwargs
    )
