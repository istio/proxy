# Copyright 2021 The Bazel Authors. All rights reserved.
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

"""Tests for `swift_interop_hint`."""

load("//test/rules:analysis_failure_test.bzl", "analysis_failure_test")
load("//test/rules:provider_test.bzl", "provider_test")

def interop_hints_test_suite(name, tags = []):
    """Test suite for `swift_interop_hint`.

    Args:
        name: The base name to be used in targets created by this macro.
        tags: Additional tags to apply to each test.
    """
    all_tags = [name] + tags

    # Verify that a hint with only a custom module name causes the `cc_library`
    # to propagate a `SwiftInfo` info with the expected auto-generated module
    # map.
    provider_test(
        name = "{}_hint_with_custom_module_name_builds".format(name),
        expected_files = [
            "test/fixtures/interop_hints/cc_lib_custom_module_name_modulemap/_/module.modulemap",
        ],
        field = "transitive_modules.clang.module_map!",
        provider = "SwiftInfo",
        tags = all_tags,
        target_under_test = "//test/fixtures/interop_hints:import_module_name_swift",
    )

    # Verify that a hint with a custom module map file causes the `cc_library`
    # to propagate a `SwiftInfo` info with that file.
    provider_test(
        name = "{}_hint_with_custom_module_map_builds".format(name),
        expected_files = [
            "test/fixtures/interop_hints/module.modulemap",
        ],
        field = "transitive_modules.clang.module_map!",
        provider = "SwiftInfo",
        tags = all_tags,
        target_under_test = "//test/fixtures/interop_hints:import_submodule_swift",
    )

    # Verify that the build fails if a hint provides `module_map` without
    # `module_name`.
    analysis_failure_test(
        name = "{}_fails_when_module_map_provided_without_module_name".format(name),
        expected_message = "'module_name' must be specified when 'module_map' is specified.",
        tags = all_tags,
        target_under_test = "//test/fixtures/interop_hints:invalid_swift",
    )

    # Verify that an `objc_library` hinted to suppress its module does not
    # propagate a `SwiftInfo` provider at all.
    provider_test(
        name = "{}_objc_library_module_suppressed".format(name),
        does_not_propagate_provider = "SwiftInfo",
        tags = all_tags,
        target_under_test = "//test/fixtures/interop_hints:objc_library_suppressed",
        target_compatible_with = ["@platforms//os:macos"],
    )

    # Verify that an `objc_library` hinted to suppress its module does not
    # propagate a `SwiftInfo` provider even if it has Swift dependencies.
    provider_test(
        name = "{}_objc_library_module_with_swift_dep_suppressed".format(name),
        does_not_propagate_provider = "SwiftInfo",
        tags = all_tags,
        target_under_test = "//test/fixtures/interop_hints:objc_library_with_swift_dep_suppressed",
        target_compatible_with = ["@platforms//os:macos"],
    )

    native.test_suite(
        name = name,
        tags = all_tags,
    )
