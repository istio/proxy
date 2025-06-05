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

"""Tests for py_cc_toolchain."""

load("@rules_testing//lib:analysis_test.bzl", "analysis_test", "test_suite")
load("@rules_testing//lib:truth.bzl", "matching", "subjects")
load("//python/cc:py_cc_toolchain.bzl", "py_cc_toolchain")
load("//tests/support:cc_info_subject.bzl", "cc_info_subject")
load("//tests/support:py_cc_toolchain_info_subject.bzl", "PyCcToolchainInfoSubject")

_tests = []

def _test_py_cc_toolchain(name):
    analysis_test(
        name = name,
        impl = _test_py_cc_toolchain_impl,
        target = "//tests/support/cc_toolchains:fake_py_cc_toolchain_impl",
        attrs = {
            "header": attr.label(
                default = "//tests/support/cc_toolchains:fake_header.h",
                allow_single_file = True,
            ),
        },
    )

def _test_py_cc_toolchain_impl(env, target):
    env.expect.that_target(target).has_provider(platform_common.ToolchainInfo)

    toolchain = PyCcToolchainInfoSubject.new(
        target[platform_common.ToolchainInfo].py_cc_toolchain,
        meta = env.expect.meta.derive(expr = "py_cc_toolchain_info"),
    )
    toolchain.python_version().equals("3.999")

    headers_providers = toolchain.headers().providers_map()
    headers_providers.keys().contains_exactly(["CcInfo", "DefaultInfo"])

    cc_info = headers_providers.get("CcInfo", factory = cc_info_subject)

    compilation_context = cc_info.compilation_context()
    compilation_context.direct_headers().contains_exactly([
        env.ctx.file.header,
    ])
    compilation_context.direct_public_headers().contains_exactly([
        env.ctx.file.header,
    ])

    # NOTE: The include dir gets added twice, once for the source path,
    # and once for the config-specific path, but we don't care about that.
    compilation_context.system_includes().contains_at_least_predicates([
        matching.str_matches("*/fake_include"),
    ])

    default_info = headers_providers.get("DefaultInfo", factory = subjects.default_info)
    default_info.runfiles().contains_predicate(
        matching.str_matches("*/cc_toolchains/data.txt"),
    )

    libs_providers = toolchain.libs().providers_map()
    libs_providers.keys().contains_exactly(["CcInfo", "DefaultInfo"])

    cc_info = libs_providers.get("CcInfo", factory = cc_info_subject)

    cc_info.linking_context().linker_inputs().has_size(2)

    default_info = libs_providers.get("DefaultInfo", factory = subjects.default_info)
    default_info.runfiles().contains("{workspace}/tests/support/cc_toolchains/libdata.txt")
    default_info.runfiles().contains_predicate(
        matching.str_matches("/libpython3."),
    )

_tests.append(_test_py_cc_toolchain)

def _test_libs_optional(name):
    py_cc_toolchain(
        name = name + "_subject",
        libs = None,
        headers = "//tests/support/cc_toolchains:fake_headers",
        python_version = "4.5",
    )
    analysis_test(
        name = name,
        target = name + "_subject",
        impl = _test_libs_optional_impl,
    )

def _test_libs_optional_impl(env, target):
    libs = target[platform_common.ToolchainInfo].py_cc_toolchain.libs
    env.expect.that_bool(libs == None).equals(True)

_tests.append(_test_libs_optional)

def py_cc_toolchain_test_suite(name):
    test_suite(
        name = name,
        tests = _tests,
    )
