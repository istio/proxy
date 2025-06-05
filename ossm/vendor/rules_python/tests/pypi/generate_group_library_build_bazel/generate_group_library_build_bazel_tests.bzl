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
load("//python/private/pypi:generate_group_library_build_bazel.bzl", "generate_group_library_build_bazel")  # buildifier: disable=bzl-visibility

_tests = []

def _test_simple(env):
    want = """\
load("@rules_python//python:py_library.bzl", "py_library")


## Group vbap

filegroup(
    name = "vbap_whl",
    srcs = [],
    data = [
        "@pypi_oletools//:_whl",
        "@pypi_pcodedmp//:_whl",
    ],
    visibility = [
        "@pypi_oletools//:__pkg__",
        "@pypi_pcodedmp//:__pkg__",
    ],
)

py_library(
    name = "vbap_pkg",
    srcs = [],
    deps = [
        "@pypi_oletools//:_pkg",
        "@pypi_pcodedmp//:_pkg",
    ],
    visibility = [
        "@pypi_oletools//:__pkg__",
        "@pypi_pcodedmp//:__pkg__",
    ],
)
"""
    actual = generate_group_library_build_bazel(
        repo_prefix = "pypi_",
        groups = {"vbap": ["pcodedmp", "oletools"]},
    )
    env.expect.that_str(actual).equals(want)

_tests.append(_test_simple)

def _test_in_hub(env):
    want = """\
load("@rules_python//python:py_library.bzl", "py_library")


## Group vbap

filegroup(
    name = "vbap_whl",
    srcs = [],
    data = [
        "//oletools:_whl",
        "//pcodedmp:_whl",
    ],
    visibility = ["//:__subpackages__"],
)

py_library(
    name = "vbap_pkg",
    srcs = [],
    deps = [
        "//oletools:_pkg",
        "//pcodedmp:_pkg",
    ],
    visibility = ["//:__subpackages__"],
)
"""
    actual = generate_group_library_build_bazel(
        repo_prefix = "",
        groups = {"vbap": ["pcodedmp", "oletools"]},
    )
    env.expect.that_str(actual).equals(want)

_tests.append(_test_in_hub)

def generate_group_library_build_bazel_test_suite(name):
    """Create the test suite.

    Args:
        name: the name of the test suite
    """
    test_suite(name = name, basic_tests = _tests)
