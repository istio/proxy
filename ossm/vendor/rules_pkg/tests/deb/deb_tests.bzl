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
"""Helpers for pkg_deb tests."""

load("@bazel_skylib//lib:unittest.bzl", "analysistest", "asserts")

def _package_naming_test_impl(ctx):
    env = analysistest.begin(ctx)
    target_under_test = analysistest.target_under_test(env)

    ogi = target_under_test[OutputGroupInfo]

    deb_path = ogi.deb.to_list()[0].path

    # Test that the .changes file is computed correctly
    changes_path = ogi.changes.to_list()[0].path
    expected_changes_path = deb_path[0:-3] + "changes"
    asserts.equals(
        env,
        changes_path,
        expected_changes_path,
        "Changes file does not have the correct name",
    )

    # Is the generated file name what we expect
    if ctx.attr.expected_name:
        asserts.equals(
            env,
            deb_path.split("/")[-1],  # basename(path)
            ctx.attr.expected_name,
            "Deb package file name is not correct",
        )
    return analysistest.end(env)

package_naming_test = analysistest.make(
    _package_naming_test_impl,
    attrs = {
        "expected_name": attr.string(),
    },
)
