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

"""Tests for interoperability with `cc_library`-specific features."""

load("@bazel_skylib//rules:build_test.bzl", "build_test")

def cc_library_test_suite(name, tags = []):
    """Test suite for interoperability with `cc_library`-specific features.

    Args:
        name: The base name to be used in targets created by this macro.
        tags: Additional tags to apply to each test.
    """
    all_tags = [name] + tags

    # Verify that Swift can import a `cc_library` that uses `include_prefix`,
    # `strip_include_prefix`, or both.
    build_test(
        name = "{}_swift_imports_cc_library_with_include_prefix_manipulation".format(name),
        targets = [
            "//test/fixtures/cc_library:import_prefix_and_strip_prefix",
            "//test/fixtures/cc_library:import_prefix_only",
            "//test/fixtures/cc_library:import_strip_prefix_only",
        ],
        tags = all_tags,
    )

    # Verify that `swift_interop_hint.exclude_hdrs` correctly excludes headers
    # from a `cc_library` that uses `include_prefix` and/or
    # `strip_include_prefix` (i.e., both the real header and the virtual header
    # are excluded).
    build_test(
        name = "{}_swift_interop_hint_excludes_headers_with_include_prefix_manipulation".format(name),
        targets = [
            "//test/fixtures/cc_library:import_prefix_and_strip_prefix_with_exclusion",
        ],
        tags = all_tags,
    )

    native.test_suite(
        name = name,
        tags = all_tags,
    )
