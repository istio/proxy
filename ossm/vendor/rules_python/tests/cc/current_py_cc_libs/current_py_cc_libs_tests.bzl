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

"""Tests for current_py_cc_libs."""

load("@rules_cc//cc/common:cc_info.bzl", "CcInfo")
load("@rules_testing//lib:analysis_test.bzl", "analysis_test", "test_suite")
load("@rules_testing//lib:truth.bzl", "matching")
load("//tests/support:cc_info_subject.bzl", "cc_info_subject")

_tests = []

def _test_current_toolchain_libs(name):
    analysis_test(
        name = name,
        impl = _test_current_toolchain_libs_impl,
        target = "//python/cc:current_py_cc_libs",
        config_settings = {
            "//command_line_option:extra_toolchains": [str(Label("//tests/support/cc_toolchains:all"))],
        },
        attrs = {
            "lib": attr.label(
                default = "//tests/support/cc_toolchains:libpython",
                allow_single_file = True,
            ),
        },
    )

def _test_current_toolchain_libs_impl(env, target):
    # Check that the forwarded CcInfo looks vaguely correct.
    cc_info = env.expect.that_target(target).provider(
        CcInfo,
        factory = cc_info_subject,
    )
    cc_info.linking_context().linker_inputs().has_size(2)

    # Check that the forward DefaultInfo looks correct
    env.expect.that_target(target).runfiles().contains_predicate(
        matching.str_matches("*/libdata.txt"),
    )

    # The shared library should also end up in runfiles
    # The `_solib` directory is a special directory CC rules put
    # libraries into.
    env.expect.that_target(target).runfiles().contains_predicate(
        matching.str_matches("*_solib*/libpython3.so"),
    )

_tests.append(_test_current_toolchain_libs)

def _test_toolchain_is_registered_by_default(name):
    analysis_test(
        name = name,
        impl = _test_toolchain_is_registered_by_default_impl,
        target = "//python/cc:current_py_cc_libs",
    )

def _test_toolchain_is_registered_by_default_impl(env, target):
    env.expect.that_target(target).has_provider(CcInfo)

_tests.append(_test_toolchain_is_registered_by_default)

def current_py_cc_libs_test_suite(name):
    test_suite(
        name = name,
        tests = _tests,
    )
