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
load("//python/config_settings/private:py_args.bzl", "py_args")  # buildifier: disable=bzl-visibility

_tests = []

def _test_py_args_default(env):
    actual = py_args("foo", {})

    want = {
        "args": None,
        "data": None,
        "deps": None,
        "env": None,
        "main": "foo.py",
        "srcs": None,
    }
    env.expect.that_dict(actual).contains_exactly(want)

_tests.append(_test_py_args_default)

def _test_kwargs_get_consumed(env):
    kwargs = {
        "args": ["some", "args"],
        "data": ["data"],
        "deps": ["deps"],
        "env": {"key": "value"},
        "main": "__main__.py",
        "srcs": ["__main__.py"],
        "visibility": ["//visibility:public"],
    }
    actual = py_args("bar_bin", kwargs)

    want = {
        "args": ["some", "args"],
        "data": ["data"],
        "deps": ["deps"],
        "env": {"key": "value"},
        "main": "__main__.py",
        "srcs": ["__main__.py"],
    }
    env.expect.that_dict(actual).contains_exactly(want)
    env.expect.that_dict(kwargs).keys().contains_exactly(["visibility"])

_tests.append(_test_kwargs_get_consumed)

def py_args_test_suite(name):
    """Create the test suite.

    Args:
        name: the name of the test suite
    """
    test_suite(name = name, basic_tests = _tests)
