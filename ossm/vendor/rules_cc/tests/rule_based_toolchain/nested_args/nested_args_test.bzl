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
"""Tests for the cc_args rule."""

load("@bazel_skylib//rules/directory:providers.bzl", "DirectoryInfo")
load("//cc:cc_toolchain_config_lib.bzl", "flag_group", "variable_with_value")
load(
    "//cc/toolchains/impl:nested_args.bzl",
    "FORMAT_ARGS_ERR",
    "REQUIRES_EQUAL_ERR",
    "REQUIRES_MUTUALLY_EXCLUSIVE_ERR",
    "REQUIRES_NONE_ERR",
    "format_args",
    "nested_args_provider",
)
load("//tests/rule_based_toolchain:subjects.bzl", "result_fn_wrapper", "subjects")

visibility("private")

def _expect_that_nested(env, expr = None, **kwargs):
    return env.expect.that_value(
        expr = expr,
        value = result_fn_wrapper(nested_args_provider)(
            label = Label("//:args"),
            **kwargs
        ),
        factory = subjects.result(subjects.NestedArgsInfo),
    )

def _expect_that_formatted(env, args, format, must_use = [], expr = None):
    return env.expect.that_value(
        result_fn_wrapper(format_args)(args, format, must_use = must_use),
        factory = subjects.result(subjects.collection),
        expr = expr or "format_args(%r, %r)" % (args, format),
    )

def _format_args_test(env, targets):
    _expect_that_formatted(
        env,
        [
            "a % b",
            "a {{",
            "}} b",
            "a {{ b }}",
        ],
        {},
    ).ok().contains_exactly([
        "a %% b",
        "a {",
        "} b",
        "a { b }",
    ]).in_order()

    _expect_that_formatted(
        env,
        ["{foo"],
        {},
    ).err().equals('Unmatched { in "{foo"')

    _expect_that_formatted(
        env,
        ["foo}"],
        {},
    ).err().equals('Unexpected } in "foo}"')
    _expect_that_formatted(
        env,
        ["{foo}"],
        {},
    ).err().contains('Unknown variable "foo" in format string "{foo}"')

    _expect_that_formatted(
        env,
        [
            "a {var}",
            "b {directory}",
            "c {file}",
        ],
        {
            "directory": targets.directory,
            "file": targets.bin_wrapper,
            "var": targets.foo,
        },
    ).ok().contains_exactly([
        "a %{foo}",
        "b " + targets.directory[DirectoryInfo].path,
        "c " + targets.bin_wrapper[DefaultInfo].files.to_list()[0].path,
    ]).in_order()

    _expect_that_formatted(
        env,
        ["{var}", "{var}"],
        {"var": targets.foo},
    ).ok().contains_exactly(["%{foo}", "%{foo}"])

    _expect_that_formatted(
        env,
        [],
        {"var": targets.foo},
        must_use = ["var"],
    ).err().contains('"var" was not used')

    _expect_that_formatted(
        env,
        ["{var} {var}"],
        {"var": targets.foo},
    ).err().contains('"{var} {var}" contained multiple variables')

    _expect_that_formatted(
        env,
        ["{foo} {bar}"],
        {"bar": targets.foo, "foo": targets.foo},
    ).err().contains('"{foo} {bar}" contained multiple variables')

def _iterate_over_test(env, targets):
    inner = _expect_that_nested(
        env,
        args = ["--foo"],
    ).ok().actual
    env.expect.that_str(inner.legacy_flag_group).equals(flag_group(flags = ["--foo"]))

    nested = _expect_that_nested(
        env,
        nested = [inner],
        iterate_over = targets.my_list,
    ).ok()
    nested.iterate_over().some().equals("my_list")
    nested.legacy_flag_group().equals(flag_group(
        iterate_over = "my_list",
        flag_groups = [inner.legacy_flag_group],
    ))
    nested.requires_types().contains_exactly({})

def _requires_types_test(env, targets):
    _expect_that_nested(
        env,
        requires_not_none = "abc",
        requires_none = "def",
        args = ["--foo"],
        expr = "mutually_exclusive",
    ).err().equals(REQUIRES_MUTUALLY_EXCLUSIVE_ERR)

    _expect_that_nested(
        env,
        requires_none = "var",
        args = ["--foo"],
        expr = "requires_none",
    ).ok().requires_types().contains_exactly(
        {"var": [struct(
            msg = REQUIRES_NONE_ERR,
            valid_types = ["option"],
            after_option_unwrap = False,
        )]},
    )

    _expect_that_nested(
        env,
        args = ["foo {foo} baz"],
        format = {targets.foo: "foo"},
        expr = "type_validation",
    ).ok().requires_types().contains_exactly(
        {"foo": [struct(
            msg = FORMAT_ARGS_ERR,
            valid_types = ["string", "file", "directory"],
            after_option_unwrap = True,
        )]},
    )

    nested = _expect_that_nested(
        env,
        requires_equal = "foo",
        requires_equal_value = "value",
        args = ["--foo={foo}"],
        format = {targets.foo: "foo"},
        expr = "type_and_requires_equal_validation",
    ).ok()
    nested.requires_types().contains_exactly(
        {"foo": [
            struct(
                msg = REQUIRES_EQUAL_ERR,
                valid_types = ["string"],
                after_option_unwrap = True,
            ),
            struct(
                msg = FORMAT_ARGS_ERR,
                valid_types = ["string", "file", "directory"],
                after_option_unwrap = True,
            ),
        ]},
    )
    nested.legacy_flag_group().equals(flag_group(
        expand_if_equal = variable_with_value(name = "foo", value = "value"),
        flags = ["--foo=%{foo}"],
    ))

TARGETS = [
    ":foo",
    ":my_list",
    "//tests/rule_based_toolchain/testdata:directory",
    "//tests/rule_based_toolchain/testdata:bin_wrapper",
]

TESTS = {
    "format_args_test": _format_args_test,
    "iterate_over_test": _iterate_over_test,
    "requires_types_test": _requires_types_test,
}
