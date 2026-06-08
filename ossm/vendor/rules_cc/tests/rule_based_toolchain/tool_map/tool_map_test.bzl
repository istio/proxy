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
"""Tests for the cc_tool_map rule."""

load("//cc/toolchains:cc_toolchain_info.bzl", "ActionTypeInfo", "ToolConfigInfo")
load("//cc/toolchains:tool_map.bzl", "cc_tool_map")
load("//tests/rule_based_toolchain:subjects.bzl", "subjects")
load("//tests/rule_based_toolchain:testing_rules.bzl", "analysis_test", "expect_failure_test", "helper_target")

_ALL_ACTIONS = "//cc/toolchains/actions:all_actions"
_C_COMPILE = "//cc/toolchains/actions:c_compile"
_CPP_COMPILE = "//cc/toolchains/actions:cpp_compile"
_ALL_CPP_COMPILE = "//cc/toolchains/actions:cpp_compile_actions"
_STRIP = "//cc/toolchains/actions:strip"
_LINK_DYNAMIC_LIBRARY = "//cc/toolchains/actions:cpp_link_executable"
_BIN = "//tests/rule_based_toolchain/testdata:bin"
_BIN_WRAPPER = "//tests/rule_based_toolchain/testdata:bin_wrapper"

def valid_config_test(name):
    subject_name = "_%s_subject" % name
    cc_tool_map(
        name = subject_name,
        tools = {
            _LINK_DYNAMIC_LIBRARY: _BIN,
            _C_COMPILE: _BIN_WRAPPER,
            _ALL_CPP_COMPILE: _BIN,
        },
    )

    analysis_test(
        name = name,
        impl = _valid_config_test_impl,
        targets = {
            "c_compile": _C_COMPILE,
            "cpp_compile": _CPP_COMPILE,
            "link_dynamic_library": _LINK_DYNAMIC_LIBRARY,
            "strip": _STRIP,
            "subject": subject_name,
        },
    )

def _valid_config_test_impl(env, targets):
    configs = env.expect.that_target(targets.subject).provider(ToolConfigInfo).configs()

    configs.contains(targets.strip[ActionTypeInfo]).equals(False)
    configs.get(targets.c_compile[ActionTypeInfo]).exe().path().split("/").offset(-1, subjects.str).equals("bin_wrapper")
    configs.get(targets.cpp_compile[ActionTypeInfo]).exe().path().split("/").offset(-1, subjects.str).equals("bin")
    configs.get(targets.link_dynamic_library[ActionTypeInfo]).exe().path().split("/").offset(-1, subjects.str).equals("bin")

def duplicate_action_test(name):
    subject_name = "_%s_subject" % name
    helper_target(
        cc_tool_map,
        name = subject_name,
        tools = {
            _C_COMPILE: _BIN_WRAPPER,
            _ALL_ACTIONS: _BIN,
        },
    )

    expect_failure_test(
        name = name,
        target = subject_name,
        failure_message = "appears multiple times in your tool_map",
    )
