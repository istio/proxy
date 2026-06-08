# Copyright 2023 The Bazel Authors. All rights reserved.
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

"""Tests for current_py_cc_headers."""

load("@rules_cc//cc/common:cc_info.bzl", "CcInfo")
load("@rules_testing//lib:analysis_test.bzl", "analysis_test", "test_suite")
load("@rules_testing//lib:truth.bzl", "matching")
load("//tests/support:cc_info_subject.bzl", "cc_info_subject")
load("//tests/support:support.bzl", "CC_TOOLCHAIN")

_tests = []

def _verify_headers_target(env, target):
    # Check that the forwarded CcInfo looks vaguely correct.
    compilation_context = env.expect.that_target(target).provider(
        CcInfo,
        factory = cc_info_subject,
    ).compilation_context()
    compilation_context.direct_headers().contains_exactly(
        env.ctx.files.header_files,
    )
    compilation_context.direct_public_headers().contains_exactly(
        env.ctx.files.header_files,
    )

    # NOTE: Bazel 8 and lower put cc_library.includes into `.system_includes`,
    # while Bazel 9 put it in `.includes`. Both result in the includes being
    # added as system includes, so either is acceptable for the expected
    # `#include <Python.h>` to work.
    includes = compilation_context.actual.includes.to_list() + compilation_context.actual.system_includes.to_list()

    # NOTE: The include dir gets added twice, once for the source path,
    # and once for the config-specific path.
    env.expect.that_collection(includes).contains_at_least_predicates([
        matching.str_matches("*/py_include"),
    ])

    # Check that the forward DefaultInfo looks correct
    env.expect.that_target(target).runfiles().contains_predicate(
        matching.str_matches("*/cc_toolchains/data.txt"),
    )

def _test_current_toolchain_headers(name):
    analysis_test(
        name = name,
        impl = _test_current_toolchain_headers_impl,
        target = "//python/cc:current_py_cc_headers",
        config_settings = {
            "//command_line_option:extra_toolchains": [CC_TOOLCHAIN],
        },
        attrs = {
            "header_files": attr.label_list(
                default = [
                    "//tests/support/cc_toolchains:py_headers_files",
                ],
                allow_files = True,
            ),
        },
    )

def _test_current_toolchain_headers_impl(env, target):
    _verify_headers_target(env, target)

_tests.append(_test_current_toolchain_headers)

def _test_toolchain_is_registered_by_default(name):
    analysis_test(
        name = name,
        impl = _test_toolchain_is_registered_by_default_impl,
        target = "//python/cc:current_py_cc_headers",
    )

def _test_toolchain_is_registered_by_default_impl(env, target):
    env.expect.that_target(target).has_provider(CcInfo)

_tests.append(_test_toolchain_is_registered_by_default)

def _test_current_toolchain_headers_abi3(name):
    analysis_test(
        name = name,
        impl = _test_current_toolchain_headers_abi3_impl,
        target = "//python/cc:current_py_cc_headers_abi3",
        config_settings = {
            "//command_line_option:extra_toolchains": [CC_TOOLCHAIN],
        },
        attrs = {
            "header_files": attr.label_list(
                default = [
                    "//tests/support/cc_toolchains:py_headers_abi3_files",
                ],
                allow_files = True,
            ),
        },
    )

def _test_current_toolchain_headers_abi3_impl(env, target):
    _verify_headers_target(env, target)

_tests.append(_test_current_toolchain_headers_abi3)

def current_py_cc_headers_test_suite(name):
    test_suite(
        name = name,
        tests = _tests,
    )
