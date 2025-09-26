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

"""Tests for file mapping routines in pkg/mappings.bzl.

Test implementation copied from pkg/mappings.bzl

"""

load("@//pkg:mappings.bzl", "pkg_files", "strip_prefix")
load("@//pkg:providers.bzl", "PackageFilesInfo")
load("@bazel_skylib//lib:new_sets.bzl", "sets")
load("@bazel_skylib//lib:unittest.bzl", "analysistest", "asserts")

#### BEGIN copied code

def _pkg_files_contents_test_impl(ctx):
    env = analysistest.begin(ctx)
    target_under_test = analysistest.target_under_test(env)

    expected_dests = sets.make(ctx.attr.expected_dests)
    actual_dests = sets.make(target_under_test[PackageFilesInfo].dest_src_map.keys())

    asserts.new_set_equals(env, expected_dests, actual_dests, "pkg_files dests do not match expectations")

    return analysistest.end(env)

pkg_files_contents_test = analysistest.make(
    _pkg_files_contents_test_impl,
    attrs = {
        # Other attributes can be tested here, but the most important one is the
        # destinations.
        "expected_dests": attr.string_list(
            mandatory = True,
        ),
        # attrs are always passed through unchanged (and maybe rejected)
    },
)

#### END copied code

# Called from the rules_pkg tests
def test_referencing_remote_file(name):
    pkg_files(
        name = "{}_g".format(name),
        prefix = "usr/share",
        srcs = ["@//tests:loremipsum_txt"],
        # The prefix in rules_pkg.  Why yes, this is knotty
        strip_prefix = strip_prefix.from_root("tests"),
        tags = ["manual"],
    )

    pkg_files_contents_test(
        name = name,
        target_under_test = ":{}_g".format(name),
        expected_dests = ["usr/share/testdata/loremipsum.txt"],
    )
