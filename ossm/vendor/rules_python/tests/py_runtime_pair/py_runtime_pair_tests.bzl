# Copyright 2023 The Bazel Authors. All rights reserved.
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
"""Starlark tests for py_runtime_pair rule."""

load("@rules_testing//lib:analysis_test.bzl", "analysis_test")
load("@rules_testing//lib:test_suite.bzl", "test_suite")
load("@rules_testing//lib:truth.bzl", "matching", "subjects")
load("@rules_testing//lib:util.bzl", rt_util = "util")
load("//python:py_binary.bzl", "py_binary")
load("//python:py_runtime.bzl", "py_runtime")
load("//python:py_runtime_pair.bzl", "py_runtime_pair")
load("//python/private:reexports.bzl", "BuiltinPyRuntimeInfo")  # buildifier: disable=bzl-visibility
load("//tests/support:py_runtime_info_subject.bzl", "py_runtime_info_subject")
load("//tests/support:support.bzl", "CC_TOOLCHAIN")

def _toolchain_factory(value, meta):
    return subjects.struct(
        value,
        meta = meta,
        attrs = {
            "py3_runtime": py_runtime_info_subject,
        },
    )

def _provides_builtin_py_runtime_info_impl(ctx):  # @unused
    return [BuiltinPyRuntimeInfo(
        python_version = "PY3",
        interpreter_path = "builtin",
    )]

_provides_builtin_py_runtime_info = rule(
    implementation = _provides_builtin_py_runtime_info_impl,
)

_tests = []

def _test_basic(name):
    rt_util.helper_target(
        py_runtime,
        name = name + "_runtime",
        interpreter = "fake_interpreter",
        python_version = "PY3",
        files = ["file1.txt"],
    )
    rt_util.helper_target(
        py_runtime_pair,
        name = name + "_subject",
        py3_runtime = name + "_runtime",
    )
    analysis_test(
        name = name,
        target = name + "_subject",
        impl = _test_basic_impl,
    )

def _test_basic_impl(env, target):
    toolchain = env.expect.that_target(target).provider(
        platform_common.ToolchainInfo,
        factory = _toolchain_factory,
    )
    toolchain.py3_runtime().python_version().equals("PY3")
    toolchain.py3_runtime().files().contains_predicate(matching.file_basename_equals("file1.txt"))
    toolchain.py3_runtime().interpreter().path().contains("fake_interpreter")

_tests.append(_test_basic)

def _test_builtin_py_info_accepted(name):
    if not BuiltinPyRuntimeInfo:
        rt_util.skip_test(name = name)
        return
    rt_util.helper_target(
        _provides_builtin_py_runtime_info,
        name = name + "_runtime",
    )
    rt_util.helper_target(
        py_runtime_pair,
        name = name + "_subject",
        py3_runtime = name + "_runtime",
    )
    analysis_test(
        name = name,
        target = name + "_subject",
        impl = _test_builtin_py_info_accepted_impl,
    )

def _test_builtin_py_info_accepted_impl(env, target):
    toolchain = env.expect.that_target(target).provider(
        platform_common.ToolchainInfo,
        factory = _toolchain_factory,
    )
    toolchain.py3_runtime().python_version().equals("PY3")
    toolchain.py3_runtime().interpreter_path().equals("builtin")

_tests.append(_test_builtin_py_info_accepted)

def _test_py_runtime_pair_and_binary(name):
    rt_util.helper_target(
        py_runtime,
        name = name + "_runtime",
        interpreter_path = "/fake_interpreter",
        python_version = "PY3",
    )
    rt_util.helper_target(
        py_runtime_pair,
        name = name + "_pair",
        py3_runtime = name + "_runtime",
    )
    native.toolchain(
        name = name + "_toolchain",
        toolchain = name + "_pair",
        toolchain_type = "//python:toolchain_type",
    )
    rt_util.helper_target(
        py_binary,
        name = name + "_subject",
        srcs = [name + "_subject.py"],
    )
    analysis_test(
        name = name,
        target = name + "_subject",
        impl = _test_py_runtime_pair_and_binary_impl,
        config_settings = {
            "//command_line_option:extra_toolchains": [
                "//tests/py_runtime_pair:{}_toolchain".format(name),
                CC_TOOLCHAIN,
            ],
        },
    )

def _test_py_runtime_pair_and_binary_impl(env, target):
    # Building indicates success, so nothing to assert
    _ = env, target  # @unused

_tests.append(_test_py_runtime_pair_and_binary)

def py_runtime_pair_test_suite(name):
    test_suite(
        name = name,
        tests = _tests,
    )
