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
load(
    "//cc:cc_toolchain_config_lib.bzl",
    "env_entry",
    "env_set",
    "flag_group",
    "flag_set",
)
load(
    "//cc/toolchains:cc_toolchain_info.bzl",
    "ActionTypeInfo",
    "ArgsInfo",
    "ArgsListInfo",
)
load(
    "//cc/toolchains/impl:legacy_converter.bzl",
    "convert_args",
)
load(
    "//cc/toolchains/impl:nested_args.bzl",
    "format_dict_values",
)
load("//tests/rule_based_toolchain:generics.bzl", "struct_subject")
load(
    "//tests/rule_based_toolchain:subjects.bzl",
    "result_fn_wrapper",
    "subjects",
)

visibility("private")

_SIMPLE_FILES = [
    "tests/rule_based_toolchain/testdata/file1",
    "tests/rule_based_toolchain/testdata/multiple1",
    "tests/rule_based_toolchain/testdata/multiple2",
]
_TOOL_DIRECTORY = "tests/rule_based_toolchain/testdata"

_CONVERTED_ARGS = subjects.struct(
    flag_sets = subjects.collection,
    env_sets = subjects.collection,
)

def _simple_test(env, targets):
    simple = env.expect.that_target(targets.simple).provider(ArgsInfo)
    simple.actions().contains_exactly([
        targets.c_compile.label,
        targets.cpp_compile.label,
    ])
    simple.env().entries().contains_exactly({"BAR": "bar"})
    simple.files().contains_exactly(_SIMPLE_FILES)

    c_compile = env.expect.that_target(targets.simple).provider(ArgsListInfo).by_action().get(
        targets.c_compile[ActionTypeInfo],
    )
    c_compile.args().contains_exactly([targets.simple[ArgsInfo]])
    c_compile.files().contains_exactly(_SIMPLE_FILES)

    converted = env.expect.that_value(
        convert_args(targets.simple[ArgsInfo]),
        factory = _CONVERTED_ARGS,
    )
    converted.env_sets().contains_exactly([env_set(
        actions = ["c_compile", "cpp_compile"],
        env_entries = [env_entry(key = "BAR", value = "bar")],
    )])

    converted.flag_sets().contains_exactly([flag_set(
        actions = ["c_compile", "cpp_compile"],
        flag_groups = [flag_group(flags = ["--foo", "foo"])],
    )])

def _env_only_test(env, targets):
    env_only = env.expect.that_target(targets.env_only).provider(ArgsInfo)
    env_only.actions().contains_exactly([
        targets.c_compile.label,
        targets.cpp_compile.label,
    ])
    env_only.env().entries().contains_exactly({"BAR": "bar"})
    env_only.files().contains_exactly(_SIMPLE_FILES)

    c_compile = env.expect.that_target(targets.simple).provider(ArgsListInfo).by_action().get(
        targets.c_compile[ActionTypeInfo],
    )
    c_compile.files().contains_exactly(_SIMPLE_FILES)

    converted = env.expect.that_value(
        convert_args(targets.env_only[ArgsInfo]),
        factory = _CONVERTED_ARGS,
    )
    converted.env_sets().contains_exactly([env_set(
        actions = ["c_compile", "cpp_compile"],
        env_entries = [env_entry(key = "BAR", value = "bar")],
    )])

    converted.flag_sets().contains_exactly([])

def _env_only_requires_test(env, targets):
    env_only = env.expect.that_target(targets.env_only_requires).provider(ArgsInfo)
    env_only.actions().contains_at_least([
        Label("//cc/toolchains/actions:c_compile"),
        Label("//cc/toolchains/actions:cpp_compile"),
    ])
    env_only.env().entries().contains_exactly(
        {"BAR": "%{dependency_file}"},
    )

    converted = env.expect.that_value(
        convert_args(targets.env_only_requires[ArgsInfo]),
        factory = _CONVERTED_ARGS,
    )

    converted.env_sets().contains_exactly([env_set(
        actions = [
            "assemble",
            "c++-compile",
            "c++-header-parsing",
            "c++-module-codegen",
            "c++-module-compile",
            "c-compile",
            "clif-match",
            "linkstamp-compile",
            "lto-backend",
            "objc++-compile",
            "objc-compile",
            "preprocess-assemble",
        ],
        env_entries = [env_entry(
            key = "BAR",
            value = "%{dependency_file}",
            expand_if_available = "dependency_file",
        )],
    )])

    converted.flag_sets().contains_exactly([])

def _with_dir_test(env, targets):
    with_dir = env.expect.that_target(targets.with_dir).provider(ArgsInfo)
    with_dir.allowlist_include_directories().contains_exactly([_TOOL_DIRECTORY])
    with_dir.files().contains_at_least(_SIMPLE_FILES)

    c_compile = env.expect.that_target(targets.with_dir).provider(ArgsListInfo).by_action().get(
        targets.c_compile[ActionTypeInfo],
    )
    c_compile.files().contains_at_least(_SIMPLE_FILES)

TARGETS = [
    ":simple",
    ":some_variable",
    ":env_only",
    ":env_only_requires",
    ":with_dir",
    ":iterate_over_optional",
    ":good_env_format",
    ":good_env_format_optional",
    "//tests/rule_based_toolchain/actions:c_compile",
    "//tests/rule_based_toolchain/actions:cpp_compile",
    "//tests/rule_based_toolchain/testdata:directory",
    "//tests/rule_based_toolchain/testdata:subdirectory_1",
    "//tests/rule_based_toolchain/testdata:bin_wrapper",
]

def _format_dict_values(args, format, must_use = [], fail = fail):
    # return the formatted dict as a list because the test framework
    # doesn't appear to support dicts
    formatted, used_items = format_dict_values(args, format, must_use = must_use, fail = fail)
    return struct(
        env = formatted.items(),
        used_items = used_items,
    )

def _expect_that_formatted(env, args, format, must_use = [], expr = None):
    return env.expect.that_value(
        result_fn_wrapper(_format_dict_values)(args, format, must_use = must_use),
        factory = subjects.result(struct_subject(
            env = subjects.collection,
            used_items = subjects.collection,
        )),
        expr = expr or "format_dict_values(%r, %r)" % (args, format),
    )

def _format_dict_values_test(env, targets):
    res = _expect_that_formatted(
        env,
        {"foo": "bar"},
        {},
    ).ok()
    res.env().contains_exactly([
        ("foo", "bar"),
    ])
    res.used_items().contains_exactly([])

    res = _expect_that_formatted(
        env,
        {"foo": "{bar}"},
        {"bar": targets.directory},
    ).ok()
    res.env().contains_exactly([
        ("foo", targets.directory[DirectoryInfo].path),
    ])
    res.used_items().contains_exactly(["bar"])

    res = _expect_that_formatted(
        env,
        {"foo": "{bar}"},
        {"bar": targets.bin_wrapper},
    ).ok()
    res.env().contains_exactly([
        ("foo", targets.bin_wrapper[DefaultInfo].files.to_list()[0].path),
    ])
    res.used_items().contains_exactly(["bar"])

    res = _expect_that_formatted(
        env,
        {
            "bat": "{quuz}",
            "baz": "{qux}",
            "foo": "{bar}",
        },
        {
            "bar": targets.directory,
            "quuz": targets.subdirectory_1,
            "qux": targets.bin_wrapper,
        },
    ).ok()
    res.env().contains_exactly([
        ("foo", targets.directory[DirectoryInfo].path),
        ("baz", targets.bin_wrapper[DefaultInfo].files.to_list()[0].path),
        ("bat", targets.subdirectory_1[DirectoryInfo].path),
    ])
    res.used_items().contains_exactly(["bar", "quuz", "qux"])

    _expect_that_formatted(
        env,
        {"foo": "{bar}"},
        {"bar": targets.some_variable},
    ).ok()

    _expect_that_formatted(
        env,
        {"foo": "{bar"},
        {},
    ).err().equals('Unmatched { in "{bar"')

    _expect_that_formatted(
        env,
        {"foo": "bar}"},
        {},
    ).err().equals('Unexpected } in "bar}"')

    _expect_that_formatted(
        env,
        {"foo": "{bar}"},
        {},
    ).err().contains('Unknown variable "bar" in format string "{bar}"')

    _expect_that_formatted(
        env,
        {"foo": "{var} {var}"},
        {"var": targets.directory},
    ).err().contains('"{var} {var}" contained multiple variables')

    _expect_that_formatted(
        env,
        {},
        {"var": targets.some_variable},
        must_use = ["var"],
    ).err().contains('"var" was not used')

def _good_env_format_test(env, targets):
    good_env = env.expect.that_target(targets.good_env_format).provider(ArgsInfo)
    good_env.env().entries().contains_exactly({"FOO": "%{gcov_gcno_file}"})

    converted = env.expect.that_value(
        convert_args(targets.good_env_format[ArgsInfo]),
        factory = _CONVERTED_ARGS,
    )
    converted.env_sets().contains_exactly([env_set(
        actions = [
            "assemble",
            "c++-compile",
            "c++-header-parsing",
            "c++-module-codegen",
            "c++-module-compile",
            "c-compile",
            "clif-match",
            "linkstamp-compile",
            "lto-backend",
            "objc++-compile",
            "objc-compile",
            "preprocess-assemble",
        ],
        env_entries = [env_entry(
            key = "FOO",
            value = "%{gcov_gcno_file}",
        )],
    )])

def _good_env_format_optional_test(env, targets):
    """Test that env formatting works with optional types."""
    good_env_optional = env.expect.that_target(targets.good_env_format_optional).provider(ArgsInfo)
    good_env_optional.env().entries().contains_exactly({"FOO": "%{dependency_file}"})

    converted = env.expect.that_value(
        convert_args(targets.good_env_format_optional[ArgsInfo]),
        factory = _CONVERTED_ARGS,
    )
    converted.env_sets().contains_exactly([env_set(
        actions = [
            "assemble",
            "c++-compile",
            "c++-header-parsing",
            "c++-module-codegen",
            "c++-module-compile",
            "c-compile",
            "clif-match",
            "linkstamp-compile",
            "lto-backend",
            "objc++-compile",
            "objc-compile",
            "preprocess-assemble",
        ],
        env_entries = [env_entry(
            key = "FOO",
            value = "%{dependency_file}",
            expand_if_available = "dependency_file",
        )],
    )])

# @unsorted-dict-items
TESTS = {
    "simple_test": _simple_test,
    "format_dict_values_test": _format_dict_values_test,
    "env_only_test": _env_only_test,
    "env_only_requires_test": _env_only_requires_test,
    "with_dir_test": _with_dir_test,
    "good_env_format_test": _good_env_format_test,
    "good_env_format_optional_test": _good_env_format_optional_test,
}
