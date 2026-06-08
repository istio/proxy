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

"""Tests for get_release_info."""

load("@rules_testing//lib:test_suite.bzl", "test_suite")
load("//python:versions.bzl", "get_release_info")  # buildifier: disable=bzl-visibility

_tests = []

def _test_file_url(env):
    """Tests that a file:/// url is handled correctly."""
    tool_versions = {
        "3.11.5": {
            "sha256": {
                "x86_64-unknown-linux-gnu": "fbed6f7694b2faae5d7c401a856219c945397f772eea5ca50c6eb825cbc9d1e1",
            },
            "strip_prefix": "python",
            "url": "file:///tmp/cpython-3.11.5.tar.gz",
        },
    }

    expected_url = "file:///tmp/cpython-3.11.5.tar.gz"
    expected_filename = "file:///tmp/cpython-3.11.5.tar.gz"

    filename, urls, strip_prefix, patches, patch_strip = get_release_info(
        platform = "x86_64-unknown-linux-gnu",
        python_version = "3.11.5",
        tool_versions = tool_versions,
    )

    env.expect.that_str(filename).equals(expected_filename)
    env.expect.that_collection(urls).contains_exactly([expected_url])
    env.expect.that_str(strip_prefix).equals("python")
    env.expect.that_collection(patches).has_size(0)
    env.expect.that_bool(patch_strip == None).equals(True)

_tests.append(_test_file_url)

def get_release_info_test_suite(name):
    """Defines the test suite for get_release_info."""
    test_suite(
        name = name,
        basic_tests = _tests,
    )
