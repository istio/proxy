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
"""Test for py_wheel."""

load("@rules_testing//lib:analysis_test.bzl", "test_suite")
load("//python/private:envsubst.bzl", "envsubst")  # buildifier: disable=bzl-visibility

_basic_tests = []

def _test_envsubst_braceless(env):
    env.expect.that_str(
        envsubst("--retries=$PIP_RETRIES", ["PIP_RETRIES"], {"PIP_RETRIES": "5"}.get),
    ).equals("--retries=5")

    env.expect.that_str(
        envsubst("--retries=$PIP_RETRIES", [], {"PIP_RETRIES": "5"}.get),
    ).equals("--retries=$PIP_RETRIES")

    env.expect.that_str(
        envsubst("--retries=$PIP_RETRIES", ["PIP_RETRIES"], {}.get),
    ).equals("--retries=")

_basic_tests.append(_test_envsubst_braceless)

def _test_envsubst_braces_without_default(env):
    env.expect.that_str(
        envsubst("--retries=${PIP_RETRIES}", ["PIP_RETRIES"], {"PIP_RETRIES": "5"}.get),
    ).equals("--retries=5")

    env.expect.that_str(
        envsubst("--retries=${PIP_RETRIES}", [], {"PIP_RETRIES": "5"}.get),
    ).equals("--retries=${PIP_RETRIES}")

    env.expect.that_str(
        envsubst("--retries=${PIP_RETRIES}", ["PIP_RETRIES"], {}.get),
    ).equals("--retries=")

_basic_tests.append(_test_envsubst_braces_without_default)

def _test_envsubst_braces_with_default(env):
    env.expect.that_str(
        envsubst("--retries=${PIP_RETRIES:-6}", ["PIP_RETRIES"], {"PIP_RETRIES": "5"}.get),
    ).equals("--retries=5")

    env.expect.that_str(
        envsubst("--retries=${PIP_RETRIES:-6}", [], {"PIP_RETRIES": "5"}.get),
    ).equals("--retries=${PIP_RETRIES:-6}")

    env.expect.that_str(
        envsubst("--retries=${PIP_RETRIES:-6}", ["PIP_RETRIES"], {}.get),
    ).equals("--retries=6")

_basic_tests.append(_test_envsubst_braces_with_default)

def _test_envsubst_nested_both_vars(env):
    env.expect.that_str(
        envsubst(
            "${HOME:-/home/$USER}",
            ["HOME", "USER"],
            {"HOME": "/home/testuser", "USER": "mockuser"}.get,
        ),
    ).equals("/home/testuser")

_basic_tests.append(_test_envsubst_nested_both_vars)

def _test_envsubst_nested_outer_var(env):
    env.expect.that_str(
        envsubst(
            "${HOME:-/home/$USER}",
            ["HOME"],
            {"HOME": "/home/testuser", "USER": "mockuser"}.get,
        ),
    ).equals("/home/testuser")

_basic_tests.append(_test_envsubst_nested_outer_var)

def _test_envsubst_nested_no_vars(env):
    env.expect.that_str(
        envsubst(
            "${HOME:-/home/$USER}",
            [],
            {"HOME": "/home/testuser", "USER": "mockuser"}.get,
        ),
    ).equals("${HOME:-/home/$USER}")

    env.expect.that_str(
        envsubst("${HOME:-/home/$USER}", ["HOME", "USER"], {}.get),
    ).equals("/home/")

_basic_tests.append(_test_envsubst_nested_no_vars)

def _test_envsubst_nested_braces_inner_var(env):
    env.expect.that_str(
        envsubst(
            "Home directory is ${HOME:-/home/$USER}.",
            ["HOME", "USER"],
            {"USER": "mockuser"}.get,
        ),
    ).equals("Home directory is /home/mockuser.")

    env.expect.that_str(
        envsubst(
            "Home directory is ${HOME:-/home/$USER}.",
            ["USER"],
            {"USER": "mockuser"}.get,
        ),
    ).equals("Home directory is ${HOME:-/home/mockuser}.")

_basic_tests.append(_test_envsubst_nested_braces_inner_var)

def envsubst_test_suite(name):
    test_suite(
        name = name,
        basic_tests = _basic_tests,
    )
