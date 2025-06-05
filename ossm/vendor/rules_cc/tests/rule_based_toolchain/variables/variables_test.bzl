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
"""Tests for variables rule."""

load("//cc/toolchains:cc_toolchain_info.bzl", "ActionTypeInfo", "BuiltinVariablesInfo", "NestedArgsInfo", "VariableInfo")
load("//cc/toolchains/impl:args_utils.bzl", _validate_nested_args = "validate_nested_args")
load(
    "//cc/toolchains/impl:nested_args.bzl",
    "FORMAT_ARGS_ERR",
    "REQUIRES_TRUE_ERR",
)
load("//cc/toolchains/impl:variables.bzl", "types", _get_type = "get_type")
load("//tests/rule_based_toolchain:subjects.bzl", "result_fn_wrapper", "subjects")

visibility("private")

get_type = result_fn_wrapper(_get_type)
validate_nested_args = result_fn_wrapper(_validate_nested_args)

_ARGS_LABEL = Label("//:args")
_NESTED_LABEL = Label("//:nested_vars")

def _type(target):
    return target[VariableInfo].type

def _types_represent_correctly_test(env, targets):
    env.expect.that_str(_type(targets.str_list)["repr"]).equals("List[string]")
    env.expect.that_str(_type(targets.str_option)["repr"]).equals("Option[string]")
    env.expect.that_str(_type(targets.struct)["repr"]).equals("struct(nested_str=string, nested_str_list=List[string])")
    env.expect.that_str(_type(targets.struct_list)["repr"]).equals("List[struct(nested_str=string, nested_str_list=List[string])]")

def _get_types_test(env, targets):
    c_compile = targets.c_compile[ActionTypeInfo]
    cpp_compile = targets.cpp_compile[ActionTypeInfo]
    variables = targets.variables[BuiltinVariablesInfo].variables

    def expect_type(key, overrides = {}, expr = None, actions = []):
        return env.expect.that_value(
            get_type(
                variables = variables,
                overrides = overrides,
                args_label = _ARGS_LABEL,
                nested_label = _NESTED_LABEL,
                actions = actions,
                name = key,
            ),
            # It's not a string, it's a complex recursive type, but string
            # supports .equals, which is all we care about.
            factory = subjects.result(subjects.str),
            expr = expr or key,
        )

    expect_type("unknown").err().contains(
        """The variable unknown does not exist. Did you mean one of the following?
optional_list
str
str_list
""",
    )

    expect_type("str").ok().equals(types.string)
    expect_type("str.invalid").err().equals("""Attempted to access "str.invalid", but "str" was not a struct - it had type string.""")

    expect_type("str_option").ok().equals(types.option(types.string))

    expect_type("str_list").ok().equals(types.list(types.string))

    expect_type("str_list.invalid").err().equals("""Attempted to access "str_list.invalid", but "str_list" was not a struct - it had type List[string].""")

    expect_type("struct").ok().equals(_type(targets.struct))

    expect_type("struct.nested_str_list").ok().equals(types.list(types.string))

    expect_type("struct_list").ok().equals(_type(targets.struct_list))

    expect_type("struct_list.nested_str_list").err().equals("""Attempted to access "struct_list.nested_str_list", but "struct_list" was not a struct - it had type List[struct(nested_str=string, nested_str_list=List[string])]. Maybe you meant to use iterate_over.""")

    expect_type("struct.unknown").err().equals("""Unable to find "unknown" in "struct", which had the following attributes:
nested_str: string
nested_str_list: List[string]""")

    expect_type("struct", actions = [c_compile]).ok()
    expect_type("struct", actions = [c_compile, cpp_compile]).err().equals(
        "The variable %s is inaccessible from the action %s. This is required because it is referenced in %s, which is included by %s, which references that action" % (targets.struct.label, cpp_compile.label, _NESTED_LABEL, _ARGS_LABEL),
    )

    expect_type("struct.nested_str_list", actions = [c_compile]).ok()
    expect_type("struct.nested_str_list", actions = [c_compile, cpp_compile]).err()

    # Simulate someone doing iterate_over = struct_list.
    expect_type(
        "struct_list",
        overrides = {"struct_list": _type(targets.struct)},
        expr = "struct_list_override",
    ).ok().equals(_type(targets.struct))

    expect_type(
        "struct_list.nested_str_list",
        overrides = {"struct_list": _type(targets.struct)},
    ).ok().equals(types.list(types.string))

    expect_type(
        "struct_list.nested_str_list",
        overrides = {
            "struct_list": _type(targets.struct),
            "struct_list.nested_str_list": types.string,
        },
    ).ok().equals(types.string)

def _variable_validation_test(env, targets):
    c_compile = targets.c_compile[ActionTypeInfo]
    cpp_compile = targets.cpp_compile[ActionTypeInfo]
    variables = targets.variables[BuiltinVariablesInfo].variables

    def _expect_validated(target, expr = None, actions = []):
        return env.expect.that_value(
            validate_nested_args(
                nested_args = target[NestedArgsInfo],
                variables = variables,
                actions = actions,
                label = _ARGS_LABEL,
            ),
            expr = expr,
            # Type is Result[None]
            factory = subjects.result(subjects.unknown),
        )

    _expect_validated(targets.simple_str, expr = "simple_str").ok()
    _expect_validated(targets.list_not_allowed).err().equals(
        FORMAT_ARGS_ERR + ", but str_list has type List[string]",
    )
    _expect_validated(targets.iterate_over_list, expr = "iterate_over_list").ok()
    _expect_validated(targets.iterate_over_non_list, expr = "iterate_over_non_list").err().equals(
        "Attempting to iterate over str, but it was not a list - it was a string",
    )
    _expect_validated(targets.str_not_a_bool, expr = "str_not_a_bool").err().equals(
        REQUIRES_TRUE_ERR + ", but str has type string",
    )
    _expect_validated(targets.str_equal, expr = "str_equal").ok()
    _expect_validated(targets.inner_iter, expr = "inner_iter_standalone").err().equals(
        'Attempted to access "struct_list.nested_str_list", but "struct_list" was not a struct - it had type List[struct(nested_str=string, nested_str_list=List[string])]. Maybe you meant to use iterate_over.',
    )

    _expect_validated(targets.outer_iter, actions = [c_compile], expr = "outer_iter_valid_action").ok()
    _expect_validated(targets.outer_iter, actions = [c_compile, cpp_compile], expr = "outer_iter_missing_action").err().equals(
        "The variable %s is inaccessible from the action %s. This is required because it is referenced in %s, which is included by %s, which references that action" % (targets.struct_list.label, cpp_compile.label, targets.outer_iter.label, _ARGS_LABEL),
    )

    _expect_validated(targets.bad_outer_iter, expr = "bad_outer_iter").err().equals(
        FORMAT_ARGS_ERR + ", but struct_list.nested_str_list has type List[string]",
    )

    _expect_validated(targets.optional_list_iter, expr = "optional_list_iter").ok()

    _expect_validated(targets.bad_nested_optional, expr = "bad_nested_optional").err().equals(
        FORMAT_ARGS_ERR + ", but str_option has type Option[string]",
    )
    _expect_validated(targets.good_nested_optional, expr = "good_nested_optional").ok()

TARGETS = [
    "//tests/rule_based_toolchain/actions:c_compile",
    "//tests/rule_based_toolchain/actions:cpp_compile",
    ":bad_nested_optional",
    ":bad_outer_iter",
    ":good_nested_optional",
    ":inner_iter",
    ":iterate_over_list",
    ":iterate_over_non_list",
    ":list_not_allowed",
    ":nested_str_list",
    ":optional_list_iter",
    ":outer_iter",
    ":simple_str",
    ":str",
    ":str_equal",
    ":str_list",
    ":str_not_a_bool",
    ":str_option",
    ":struct",
    ":struct_list",
    ":variables",
]

# @unsorted-dict-items
TESTS = {
    "types_represent_correctly_test": _types_represent_correctly_test,
    "get_types_test": _get_types_test,
    "variable_validation_test": _variable_validation_test,
}
