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
load("//python/private/pypi:whl_repo_name.bzl", "whl_repo_name")  # buildifier: disable=bzl-visibility

_tests = []

def _test_simple(env):
    got = whl_repo_name("foo-1.2.3-py3-none-any.whl", "deadbeef")
    env.expect.that_str(got).equals("foo_py3_none_any_deadbeef")

_tests.append(_test_simple)

def _test_simple_no_sha(env):
    got = whl_repo_name("foo-1.2.3-py3-none-any.whl", "")
    env.expect.that_str(got).equals("foo_1_2_3_py3_none_any")

_tests.append(_test_simple_no_sha)

def _test_sdist(env):
    got = whl_repo_name("foo-1.2.3.tar.gz", "deadbeef000deadbeef")
    env.expect.that_str(got).equals("foo_sdist_deadbeef")

_tests.append(_test_sdist)

def _test_sdist_no_sha(env):
    got = whl_repo_name("foo-1.2.3.tar.gz", "")
    env.expect.that_str(got).equals("foo_1_2_3")

_tests.append(_test_sdist_no_sha)

def _test_platform_whl(env):
    got = whl_repo_name(
        "foo-1.2.3-cp39.cp310-abi3-manylinux1_x86_64.manylinux_2_17_x86_64.whl",
        "deadbeef000deadbeef",
    )

    # We only need the first segment of each
    env.expect.that_str(got).equals("foo_cp39_abi3_manylinux_2_5_x86_64_deadbeef")

_tests.append(_test_platform_whl)

def _test_name_with_plus(env):
    got = whl_repo_name("gptqmodel-2.0.0+cu126torch2.6-cp312-cp312-linux_x86_64.whl", "")
    env.expect.that_str(got).equals("gptqmodel_2_0_0_cu126torch2_6_cp312_cp312_linux_x86_64")

_tests.append(_test_name_with_plus)

def _test_name_with_percent(env):
    got = whl_repo_name("gptqmodel-2.0.0%2Bcu126torch2.6-cp312-cp312-linux_x86_64.whl", "")
    env.expect.that_str(got).equals("gptqmodel_2_0_0_2Bcu126torch2_6_cp312_cp312_linux_x86_64")

_tests.append(_test_name_with_percent)

def whl_repo_name_test_suite(name):
    """Create the test suite.

    Args:
        name: the name of the test suite
    """
    test_suite(name = name, basic_tests = _tests)
