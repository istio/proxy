# Copyright 2020 The Bazel Authors. All rights reserved.
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

"""Tests for rpmbuild toolchain type."""

load("@bazel_skylib//lib:unittest.bzl", "analysistest", "asserts")
load("//toolchains/rpm:rpmbuild.bzl", "rpmbuild_toolchain")

# Generic negative test boilerplate
def _generic_neg_test_impl(ctx):
    env = analysistest.begin(ctx)
    asserts.expect_failure(env, ctx.attr.reason)
    return analysistest.end(env)

generic_neg_test = analysistest.make(
    _generic_neg_test_impl,
    attrs = {
        "reason": attr.string(
            default = "",
        ),
    },
    expect_failure = True,
)

def _toolchain_contents_test_impl(ctx):
    env = analysistest.begin(ctx)
    target_under_test = analysistest.target_under_test(env)

    info = target_under_test[platform_common.ToolchainInfo].rpmbuild
    asserts.equals(
        env,
        ctx.attr.expect_valid,
        info.valid,
    )
    asserts.equals(
        env,
        ctx.attr.expect_label,
        info.label,
    )
    asserts.equals(
        env,
        ctx.attr.expect_path,
        info.path,
    )
    return analysistest.end(env)

toolchain_contents_test = analysistest.make(
    _toolchain_contents_test_impl,
    attrs = {
        "expect_valid": attr.bool(default = True),
        "expect_label": attr.label(
            cfg = "exec",
            executable = True,
            allow_files = True,
        ),
        "expect_path": attr.string(),
    },
)

def _create_toolchain_creation_tests():
    rpmbuild_toolchain(
        name = "tc_label_and_path",
        label = "foo",
        path = "bar",
        tags = ["manual"],
    )
    generic_neg_test(
        name = "tc_label_and_path_test",
        target_under_test = ":tc_label_and_path",
        reason = "rpmbuild_toolchain must not specify both label and path.",
    )

    rpmbuild_toolchain(
        name = "tc_no_label_or_path",
        tags = ["manual"],
    )
    toolchain_contents_test(
        name = "tc_no_label_or_path_test",
        target_under_test = ":tc_no_label_or_path",
        expect_valid = False,
        expect_label = None,
        expect_path = "",
    )

    rpmbuild_toolchain(
        name = "tc_just_label",
        label = ":toolchain_test.bzl",  # Using self so we have a real target.
        tags = ["manual"],
    )
    toolchain_contents_test(
        name = "tc_just_label_test",
        target_under_test = ":tc_just_label",
        expect_valid = True,
        expect_label = Label("//tests/rpm:toolchain_test.bzl"),
        expect_path = "",
    )

    rpmbuild_toolchain(
        name = "tc_just_path",
        path = "/usr/bin/foo",
        tags = ["manual"],
    )
    toolchain_contents_test(
        name = "tc_just_path_test",
        target_under_test = ":tc_just_path",
        expect_valid = True,
        expect_label = None,
        expect_path = "/usr/bin/foo",
    )

# buildifier: disable=unnamed-macro
def create_toolchain_analysis_tests():
    _create_toolchain_creation_tests()
