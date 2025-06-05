# Copyright 2020 The Bazel Authors. All rights reserved.
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

"""Tests for `SwiftInfo` propagation through non-Swift targets."""

load("//test/rules:provider_test.bzl", "provider_test")

def swift_through_non_swift_test_suite(name, tags = []):
    """Test suite for propagation of `SwiftInfo` through non-Swift targets.

    Args:
        name: The base name to be used in targets created by this macro.
        tags: Additional tags to apply to each test.
    """
    all_tags = [name] + tags

    # The lower swiftmodule should get propagated through the `objc_library` (by
    # the aspect) and up to the upper target. Make sure it wasn't dropped.
    provider_test(
        name = "{}_swiftmodules_propagate_through_non_swift_targets".format(name),
        expected_files = [
            "test_fixtures_swift_through_non_swift_lower.swiftmodule",
            "test_fixtures_swift_through_non_swift_upper.swiftmodule",
        ],
        field = "transitive_modules.swift!.swiftmodule",
        provider = "SwiftInfo",
        tags = all_tags,
        target_under_test = "//test/fixtures/swift_through_non_swift:upper",
    )

    native.test_suite(
        name = name,
        tags = all_tags,
    )
