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

load("@rules_testing//lib:analysis_test.bzl", "analysis_test")
load("@rules_testing//lib:util.bzl", rt_util = "util")
load("//python:py_test.bzl", "py_test")
load(
    "//tests/base_rules:py_executable_base_tests.bzl",
    "create_executable_tests",
)
load("//tests/base_rules:util.bzl", pt_util = "util")
load("//tests/support:support.bzl", "CC_TOOLCHAIN", "CROSSTOOL_TOP", "LINUX_X86_64", "MAC_X86_64")

# The Windows CI currently runs as root, which breaks when
# the analysis tests try to install (but not use, because
# these are analysis tests) a runtime for another platform.
# This is because the toolchain install has an assert to
# verify the runtime install is read-only, which it can't
# be when running as root.
_SKIP_WINDOWS = {
    "target_compatible_with": select({
        "@platforms//os:windows": ["@platforms//:incompatible"],
        "//conditions:default": [],
    }),
}

_tests = []

def _test_mac_requires_darwin_for_execution(name, config):
    # Bazel 5.4 has a bug where every access of testing.ExecutionInfo is
    # a different object that isn't equal to any other, which prevents
    # rules_testing from detecting it properly and fails with an error.
    # This is fixed in Bazel 6+.
    if not pt_util.is_bazel_6_or_higher():
        rt_util.skip_test(name = name)
        return

    rt_util.helper_target(
        config.rule,
        name = name + "_subject",
        srcs = [name + "_subject.py"],
    )
    analysis_test(
        name = name,
        impl = _test_mac_requires_darwin_for_execution_impl,
        target = name + "_subject",
        config_settings = {
            "//command_line_option:cpu": "darwin_x86_64",
            "//command_line_option:crosstool_top": CROSSTOOL_TOP,
            "//command_line_option:extra_toolchains": CC_TOOLCHAIN,
            "//command_line_option:platforms": [MAC_X86_64],
        },
        attr_values = _SKIP_WINDOWS,
    )

def _test_mac_requires_darwin_for_execution_impl(env, target):
    env.expect.that_target(target).provider(
        testing.ExecutionInfo,
    ).requirements().keys().contains("requires-darwin")

_tests.append(_test_mac_requires_darwin_for_execution)

def _test_non_mac_doesnt_require_darwin_for_execution(name, config):
    # Bazel 5.4 has a bug where every access of testing.ExecutionInfo is
    # a different object that isn't equal to any other, which prevents
    # rules_testing from detecting it properly and fails with an error.
    # This is fixed in Bazel 6+.
    if not pt_util.is_bazel_6_or_higher():
        rt_util.skip_test(name = name)
        return
    rt_util.helper_target(
        config.rule,
        name = name + "_subject",
        srcs = [name + "_subject.py"],
    )
    analysis_test(
        name = name,
        impl = _test_non_mac_doesnt_require_darwin_for_execution_impl,
        target = name + "_subject",
        config_settings = {
            "//command_line_option:cpu": "k8",
            "//command_line_option:crosstool_top": CROSSTOOL_TOP,
            "//command_line_option:extra_toolchains": CC_TOOLCHAIN,
            "//command_line_option:platforms": [LINUX_X86_64],
        },
        attr_values = _SKIP_WINDOWS,
    )

def _test_non_mac_doesnt_require_darwin_for_execution_impl(env, target):
    # Non-mac builds don't have the provider at all.
    if testing.ExecutionInfo not in target:
        return
    env.expect.that_target(target).provider(
        testing.ExecutionInfo,
    ).requirements().keys().not_contains("requires-darwin")

_tests.append(_test_non_mac_doesnt_require_darwin_for_execution)

def py_test_test_suite(name):
    config = struct(rule = py_test)
    native.test_suite(
        name = name,
        tests = pt_util.create_tests(_tests, config = config) + create_executable_tests(config),
    )
