# Copyright 2022 The Bazel Authors. All rights reserved.
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

"""Tests for Swift module aliases using the `:module_mapping` flag."""

load("@bazel_skylib//rules:build_test.bzl", "build_test")
load(
    "//swift:swift_module_mapping_test.bzl",
    "swift_module_mapping_test",
)

def module_mapping_test_suite(name, tags = []):
    """Tests for Swift module aliases using the `:module_mapping` flag.

    Args:
        name: The base name to be used in targets created by this macro.
        tags: Additional tags to apply to each test.
    """
    all_tags = [name] + tags

    # Verify that a `swift_binary` builds properly when depending on a
    # `swift_import` target that references a `.swiftinterface` file.
    build_test(
        name = "{}_module_mapping_is_applied".format(name),
        targets = [
            "//test/fixtures/module_mapping:MySDK_with_mapping",
        ],
        tags = all_tags,
    )

    # Verify that a `swift_module_mapping_test` with a complete mapping
    # succeeds.
    swift_module_mapping_test(
        name = "{}_module_mapping_test_succeeds_with_complete_mapping".format(name),
        mapping = "//test/fixtures/module_mapping:ExistingLibrary_module_mapping_complete",
        tags = all_tags,
        deps = [
            "//test/fixtures/module_mapping:ExistingLibrary",
        ],
    )

    # Verify that a `swift_module_mapping_test` with an incomplete mapping
    # succeeds if the missing modules are listed in `exclude`.
    swift_module_mapping_test(
        name = "{}_module_mapping_test_succeeds_with_exclusions".format(name),
        exclude = ["NewDependency"],
        mapping = "//test/fixtures/module_mapping:ExistingLibrary_module_mapping_incomplete",
        tags = all_tags,
        deps = [
            "//test/fixtures/module_mapping:ExistingLibrary",
        ],
    )

    native.test_suite(
        name = name,
        tags = all_tags,
    )
