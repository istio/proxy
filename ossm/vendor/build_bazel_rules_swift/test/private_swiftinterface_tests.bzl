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

"""Tests for interoperability with `cc_library`-specific features."""

load("@bazel_skylib//rules:build_test.bzl", "build_test")
load(
    "//test/rules:action_command_line_test.bzl",
    "make_action_command_line_test_rule",
)

private_swiftinterface_test = make_action_command_line_test_rule(
    config_settings = {
        "//command_line_option:features": [
            "swift.emit_private_swiftinterface",
        ],
    },
)

def private_swiftinterface_test_suite(name, tags = []):
    """Test suite for features that compile Swift module interfaces.

    Args:
        name: The base name to be used in targets created by this macro.
        tags: Additional tags to apply to each test.
    """
    all_tags = [name] + tags

    # Verify that a `swift_binary` builds properly when depending on a
    # `swift_import` target that references a `.private.swiftinterface` file.
    build_test(
        name = "{}_swift_binary_imports_private_swiftinterface".format(name),
        targets = [
            "//test/fixtures/private_swiftinterface:client",
        ],
        tags = all_tags,
    )

    native.test_suite(
        name = name,
        tags = all_tags,
    )
