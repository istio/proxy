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
"""Starlark tests for py_exec_tools_toolchain rule."""

load("@rules_testing//lib:analysis_test.bzl", "analysis_test")
load("@rules_testing//lib:test_suite.bzl", "test_suite")
load("//python/private:py_exec_tools_toolchain.bzl", "py_exec_tools_toolchain")  # buildifier: disable=bzl-visibility

_tests = []

def _test_disable_exec_interpreter(name):
    py_exec_tools_toolchain(
        name = name + "_subject",
        exec_interpreter = "//python/private:sentinel",
    )
    analysis_test(
        name = name,
        target = name + "_subject",
        impl = _test_disable_exec_interpreter_impl,
    )

def _test_disable_exec_interpreter_impl(env, target):
    exec_tools = target[platform_common.ToolchainInfo].exec_tools
    env.expect.that_bool(exec_tools.exec_interpreter == None).equals(True)

_tests.append(_test_disable_exec_interpreter)

def py_exec_tools_toolchain_test_suite(name):
    test_suite(name = name, tests = _tests)
