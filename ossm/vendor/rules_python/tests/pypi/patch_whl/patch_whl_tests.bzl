# Copyright 2024 The Bazel Authors. All rights reserved.
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
load("//python/private/pypi:patch_whl.bzl", "patched_whl_name")  # buildifier: disable=bzl-visibility

_tests = []

def _test_simple(env):
    got = patched_whl_name("foo-1.2.3-py3-none-any.whl")
    env.expect.that_str(got).equals("foo-1.2.3+patched-py3-none-any.whl")

_tests.append(_test_simple)

def _test_simple_local_version(env):
    got = patched_whl_name("foo-1.2.3+special-py3-none-any.whl")
    env.expect.that_str(got).equals("foo-1.2.3+special.patched-py3-none-any.whl")

_tests.append(_test_simple_local_version)

def patch_whl_test_suite(name):
    """Create the test suite.

    Args:
        name: the name of the test suite
    """
    test_suite(name = name, basic_tests = _tests)
