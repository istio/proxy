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

"""Tests for `const_values`."""

load(
    "//test/rules:action_command_line_test.bzl",
    "make_action_command_line_test_rule",
)
load(
    "//test/rules:provider_test.bzl",
    "make_provider_test_rule",
    "provider_test",
)

const_values_test = make_action_command_line_test_rule()

const_values_wmo_test = make_provider_test_rule(
    config_settings = {
        str(Label("//swift:copt")): [
            "-whole-module-optimization",
        ],
    },
)

def const_values_test_suite(name, tags = []):
    """Test suite for `swift_library` producing .swiftconstvalues files.

    Args:
        name: The base name to be used in things created by this macro.
        tags: Additional tags to apply to each test.
    """
    all_tags = [name] + tags

    provider_test(
        name = "{}_empty_const_values_single_file".format(name),
        expected_files = [
            "test/fixtures/debug_settings/simple_objs/Empty.swift.swiftconstvalues",
        ],
        field = "const_values",
        provider = "OutputGroupInfo",
        tags = all_tags,
        target_under_test = "//test/fixtures/debug_settings:simple",
    )

    provider_test(
        name = "{}_empty_const_values_multiple_files".format(name),
        expected_files = [
            "test/fixtures/multiple_files/multiple_files_objs/Empty.swift.swiftconstvalues",
            "test/fixtures/multiple_files/multiple_files_objs/Empty2.swift.swiftconstvalues",
        ],
        field = "const_values",
        provider = "OutputGroupInfo",
        tags = all_tags,
        target_under_test = "//test/fixtures/multiple_files",
    )

    const_values_wmo_test(
        name = "{}_wmo_single_values_file".format(name),
        expected_files = [
            "test/fixtures/multiple_files/multiple_files.swiftconstvalues",
        ],
        field = "const_values",
        provider = "OutputGroupInfo",
        tags = all_tags,
        target_under_test = "//test/fixtures/multiple_files",
    )

    const_values_test(
        name = "{}_expected_argv".format(name),
        expected_argv = [
            "-Xfrontend -const-gather-protocols-file",
            "-Xfrontend swift/toolchains/config/const_protocols_to_gather.json",
            "-emit-const-values-path",
            "first.swift.swiftconstvalues",
        ],
        mnemonic = "SwiftCompile",
        tags = all_tags,
        target_under_test = "//test/fixtures/basic:first",
    )

    native.test_suite(
        name = name,
        tags = all_tags,
    )
