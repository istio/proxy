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
"""Starlark tests for PyRuntimeInfo provider."""

load("@rules_testing//lib:analysis_test.bzl", "analysis_test")
load("@rules_testing//lib:test_suite.bzl", "test_suite")
load("//python:py_runtime_info.bzl", "PyRuntimeInfo")
load("//python/private:util.bzl", "IS_BAZEL_7_OR_HIGHER")  # buildifier: disable=bzl-visibility

def _create_py_runtime_info_without_interpreter_version_info_impl(ctx):
    kwargs = {}
    if IS_BAZEL_7_OR_HIGHER:
        kwargs["bootstrap_template"] = ctx.attr.bootstrap_template

    return [PyRuntimeInfo(
        interpreter = ctx.file.interpreter,
        files = depset(ctx.files.files),
        python_version = "PY3",
        **kwargs
    )]

_create_py_runtime_info_without_interpreter_version_info = rule(
    implementation = _create_py_runtime_info_without_interpreter_version_info_impl,
    attrs = {
        "bootstrap_template": attr.label(allow_single_file = True, default = "bootstrap.txt"),
        "files": attr.label_list(allow_files = True, default = ["data.txt"]),
        "interpreter": attr.label(allow_single_file = True, default = "interpreter.sh"),
        "python_version": attr.string(default = "PY3"),
    },
)

_tests = []

def _test_can_create_py_runtime_info_without_interpreter_version_info(name):
    _create_py_runtime_info_without_interpreter_version_info(
        name = name + "_subject",
    )
    analysis_test(
        name = name,
        target = name + "_subject",
        impl = _test_can_create_py_runtime_info_without_interpreter_version_info_impl,
    )

def _test_can_create_py_runtime_info_without_interpreter_version_info_impl(env, target):
    # If we get this for, construction succeeded, so nothing to check
    _ = env, target  # @unused

_tests.append(_test_can_create_py_runtime_info_without_interpreter_version_info)

def py_runtime_info_test_suite(name):
    test_suite(
        name = name,
        tests = _tests,
    )
