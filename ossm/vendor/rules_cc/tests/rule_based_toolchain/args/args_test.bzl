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
load("//tests/rule_based_toolchain:subjects.bzl", "subjects")

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
    simple.env().contains_exactly({"BAR": "bar"})
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
    env_only.env().contains_exactly({"BAR": "bar"})
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
    ":env_only",
    ":with_dir",
    ":iterate_over_optional",
    "//tests/rule_based_toolchain/actions:c_compile",
    "//tests/rule_based_toolchain/actions:cpp_compile",
]

# @unsorted-dict-items
TESTS = {
    "simple_test": _simple_test,
    "env_only_test": _env_only_test,
    "with_dir_test": _with_dir_test,
}
