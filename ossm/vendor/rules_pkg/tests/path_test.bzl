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

"""Tests for path.bzl"""

load("@bazel_skylib//lib:unittest.bzl", "analysistest", "asserts")
load("//pkg:mappings.bzl", "pkg_mkdirs")
load("//pkg:path.bzl", "compute_data_path")

##########
# Test compute_data_path
##########
def _compute_data_path_test_impl(ctx):
    env = analysistest.begin(ctx)

    # Subtle: This allows you to vendor the library into your own repo at some
    # arbitrary path.
    expect = ctx.attr.expected_path
    if expect.startswith("tests"):
        expect = ctx.label.package + expect[5:]
    asserts.equals(
        env,
        expect,
        compute_data_path(ctx.label, ctx.attr.in_path),
    )
    return analysistest.end(env)

compute_data_path_test = analysistest.make(
    _compute_data_path_test_impl,
    attrs = {
        "in_path": attr.string(mandatory = True),
        "expected_path": attr.string(mandatory = True),
    },
)

def _test_compute_data_path(name):
    pkg_mkdirs(
        name = "dummy",
        dirs = [],
        tags = ["manual"],
    )

    compute_data_path_test(
        name = name + "_normal_test",
        target_under_test = ":dummy",
        in_path = "a/b/c",
        expected_path = "tests/a/b/c",
    )

    compute_data_path_test(
        name = name + "_absolute_test",
        target_under_test = ":dummy",
        in_path = "/a/b/c",
        expected_path = "a/b/c",
    )

    compute_data_path_test(
        name = name + "_relative_test",
        target_under_test = ":dummy",
        in_path = "./a/b/c",
        expected_path = "tests/a/b/c",
    )

    compute_data_path_test(
        name = name + "_empty_test",
        target_under_test = ":dummy",
        in_path = "./",
        expected_path = "tests",
    )

    compute_data_path_test(
        name = name + "_empty2_test",
        target_under_test = ":dummy",
        in_path = "./.",
        expected_path = "tests",
    )

def path_tests(name):
    """Declare path.bzl analysis tests."""
    _test_compute_data_path(name = name + "_compute_data_path")
