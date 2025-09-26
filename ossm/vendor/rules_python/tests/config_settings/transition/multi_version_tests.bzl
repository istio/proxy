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
"""Tests for py_test."""

load("@pythons_hub//:versions.bzl", "DEFAULT_PYTHON_VERSION")
load("@rules_testing//lib:analysis_test.bzl", "analysis_test")
load("@rules_testing//lib:test_suite.bzl", "test_suite")
load("@rules_testing//lib:util.bzl", "TestingAspectInfo", rt_util = "util")
load("//python:py_binary.bzl", "py_binary")
load("//python:py_info.bzl", "PyInfo")
load("//python:py_test.bzl", "py_test")
load("//python/private:reexports.bzl", "BuiltinPyInfo")  # buildifier: disable=bzl-visibility
load("//python/private:util.bzl", "IS_BAZEL_7_OR_HIGHER")  # buildifier: disable=bzl-visibility
load("//tests/support:support.bzl", "CC_TOOLCHAIN")

# NOTE @aignas 2024-06-04: we are using here something that is registered in the MODULE.Bazel
# and if you find tests failing, it could be because of the toolchain resolution issues here.
#
# If the toolchain is not resolved then you will have a weird message telling
# you that your transition target does not have a PyRuntime provider, which is
# caused by there not being a toolchain detected for the target.
_PYTHON_VERSION = DEFAULT_PYTHON_VERSION

_tests = []

def _test_py_test_with_transition(name):
    rt_util.helper_target(
        py_test,
        name = name + "_subject",
        srcs = [name + "_subject.py"],
        python_version = _PYTHON_VERSION,
    )

    analysis_test(
        name = name,
        target = name + "_subject",
        impl = _test_py_test_with_transition_impl,
    )

def _test_py_test_with_transition_impl(env, target):
    # Nothing to assert; we just want to make sure it builds
    env.expect.that_target(target).has_provider(PyInfo)
    if BuiltinPyInfo:
        env.expect.that_target(target).has_provider(BuiltinPyInfo)

_tests.append(_test_py_test_with_transition)

def _test_py_binary_with_transition(name):
    rt_util.helper_target(
        py_binary,
        name = name + "_subject",
        srcs = [name + "_subject.py"],
        python_version = _PYTHON_VERSION,
    )

    analysis_test(
        name = name,
        target = name + "_subject",
        impl = _test_py_binary_with_transition_impl,
    )

def _test_py_binary_with_transition_impl(env, target):
    # Nothing to assert; we just want to make sure it builds
    env.expect.that_target(target).has_provider(PyInfo)
    if BuiltinPyInfo:
        env.expect.that_target(target).has_provider(BuiltinPyInfo)

_tests.append(_test_py_binary_with_transition)

def _setup_py_binary_windows(name, *, impl, build_python_zip):
    rt_util.helper_target(
        py_binary,
        name = name + "_subject",
        srcs = [name + "_subject.py"],
        python_version = _PYTHON_VERSION,
    )

    analysis_test(
        name = name,
        target = name + "_subject",
        impl = impl,
        config_settings = {
            "//command_line_option:build_python_zip": build_python_zip,
            "//command_line_option:extra_toolchains": CC_TOOLCHAIN,
            "//command_line_option:platforms": str(Label("//tests/support:windows_x86_64")),
        },
    )

def _test_py_binary_windows_build_python_zip_false(name):
    _setup_py_binary_windows(
        name,
        build_python_zip = "false",
        impl = _test_py_binary_windows_build_python_zip_false_impl,
    )

def _test_py_binary_windows_build_python_zip_false_impl(env, target):
    default_outputs = env.expect.that_target(target).default_outputs()
    if IS_BAZEL_7_OR_HIGHER:
        # TODO: These outputs aren't correct. The outputs shouldn't
        # have the "_" prefix on them (those are coming from the underlying
        # wrapped binary).
        env.expect.that_target(target).default_outputs().contains_exactly([
            "{package}/{test_name}_subject.exe",
            "{package}/{test_name}_subject",
            "{package}/{test_name}_subject.py",
        ])
    else:
        inner_exe = target[TestingAspectInfo].attrs.target[DefaultInfo].files_to_run.executable
        default_outputs.contains_at_least([
            inner_exe.short_path,
        ])

_tests.append(_test_py_binary_windows_build_python_zip_false)

def _test_py_binary_windows_build_python_zip_true(name):
    _setup_py_binary_windows(
        name,
        build_python_zip = "true",
        impl = _test_py_binary_windows_build_python_zip_true_impl,
    )

def _test_py_binary_windows_build_python_zip_true_impl(env, target):
    default_outputs = env.expect.that_target(target).default_outputs()
    if IS_BAZEL_7_OR_HIGHER:
        # TODO: These outputs aren't correct. The outputs shouldn't
        # have the "_" prefix on them (those are coming from the underlying
        # wrapped binary).
        default_outputs.contains_exactly([
            "{package}/{test_name}_subject.exe",
            "{package}/{test_name}_subject.py",
            "{package}/{test_name}_subject.zip",
        ])
    else:
        inner_exe = target[TestingAspectInfo].attrs.target[DefaultInfo].files_to_run.executable
        default_outputs.contains_at_least([
            "{package}/{test_name}_subject.zip",
            inner_exe.short_path,
        ])

_tests.append(_test_py_binary_windows_build_python_zip_true)

def multi_version_test_suite(name):
    test_suite(
        name = name,
        tests = _tests,
    )
