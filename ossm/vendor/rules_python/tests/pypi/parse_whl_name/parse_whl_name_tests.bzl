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
load("//python/private/pypi:parse_whl_name.bzl", "parse_whl_name")  # buildifier: disable=bzl-visibility

_tests = []

def _test_simple(env):
    got = parse_whl_name("foo-1.2.3-py3-none-any.whl")
    env.expect.that_str(got.distribution).equals("foo")
    env.expect.that_str(got.version).equals("1.2.3")
    env.expect.that_str(got.abi_tag).equals("none")
    env.expect.that_str(got.platform_tag).equals("any")
    env.expect.that_str(got.python_tag).equals("py3")
    env.expect.that_str(got.build_tag).equals(None)

_tests.append(_test_simple)

def _test_with_build_tag(env):
    got = parse_whl_name("foo-3.2.1-9999-py2.py3-none-any.whl")
    env.expect.that_str(got.distribution).equals("foo")
    env.expect.that_str(got.version).equals("3.2.1")
    env.expect.that_str(got.abi_tag).equals("none")
    env.expect.that_str(got.platform_tag).equals("any")
    env.expect.that_str(got.python_tag).equals("py2.py3")
    env.expect.that_str(got.build_tag).equals("9999")

_tests.append(_test_with_build_tag)

def _test_multiple_platforms(env):
    got = parse_whl_name("bar-3.2.1-py3-abi3-manylinux1.manylinux2.whl")
    env.expect.that_str(got.distribution).equals("bar")
    env.expect.that_str(got.version).equals("3.2.1")
    env.expect.that_str(got.abi_tag).equals("abi3")
    env.expect.that_str(got.platform_tag).equals("manylinux1.manylinux2")
    env.expect.that_str(got.python_tag).equals("py3")
    env.expect.that_str(got.build_tag).equals(None)

_tests.append(_test_multiple_platforms)

def _test_real_numpy_wheel(env):
    got = parse_whl_name("numpy-1.26.1-pp39-pypy39_pp73-macosx_10_9_x86_64.whl")
    env.expect.that_str(got.distribution).equals("numpy")
    env.expect.that_str(got.version).equals("1.26.1")
    env.expect.that_str(got.abi_tag).equals("pypy39_pp73")
    env.expect.that_str(got.platform_tag).equals("macosx_10_9_x86_64")
    env.expect.that_str(got.python_tag).equals("pp39")
    env.expect.that_str(got.build_tag).equals(None)

_tests.append(_test_real_numpy_wheel)

def parse_whl_name_test_suite(name):
    """Create the test suite.

    Args:
        name: the name of the test suite
    """
    test_suite(name = name, basic_tests = _tests)
