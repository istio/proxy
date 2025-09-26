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

""

load("@rules_testing//lib:test_suite.bzl", "test_suite")
load("//python/private:version_label.bzl", "version_label")  # buildifier: disable=bzl-visibility

_tests = []

def _test_version_label_from_major_minor_version(env):
    actual = version_label("3.9")
    env.expect.that_str(actual).equals("39")

_tests.append(_test_version_label_from_major_minor_version)

def _test_version_label_from_major_minor_patch_version(env):
    actual = version_label("3.9.3")
    env.expect.that_str(actual).equals("39")

_tests.append(_test_version_label_from_major_minor_patch_version)

def _test_version_label_from_major_minor_version_custom_sep(env):
    actual = version_label("3.9", sep = "_")
    env.expect.that_str(actual).equals("3_9")

_tests.append(_test_version_label_from_major_minor_version_custom_sep)

def _test_version_label_from_complex_version(env):
    actual = version_label("3.9.3-rc.0")
    env.expect.that_str(actual).equals("39")

_tests.append(_test_version_label_from_complex_version)

def version_label_test_suite(name):
    """Create the test suite.

    Args:
        name: the name of the test suite
    """
    test_suite(name = name, basic_tests = _tests)
