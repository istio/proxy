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

load("@//pkg:mappings.bzl", "pkg_files", "strip_prefix")
load("@//pkg:tar.bzl", "pkg_tar")
load("@//pkg:verify_archive.bzl", "verify_archive_test")
load("@//tests/util:defs.bzl", "fake_artifact")
load("test.bzl", "test_referencing_remote_file")

package(default_visibility = ["//visibility:public"])

exports_files(
    glob(["**"]),
)

fake_artifact(
    name = "dir/script",
    files = ["dir/extproj.sh"],
    visibility = ["//visibility:public"],
)

pkg_files(
    name = "extproj_script_pf",
    srcs = ["dir/extproj.sh"],
    prefix = "usr/bin",
    strip_prefix = strip_prefix.from_pkg(),
    tags = ["manual"],
)

test_referencing_remote_file(
    name = "pf_local_file_in_extrepo",
)

# Add a target to ensure verify_archive_test can be
# called from another workspace
pkg_tar(
    name = "external_archive",
    srcs = ["dir/extproj.sh"],
)

verify_archive_test(
    name = "external_archive_test",
    max_size = 1,
    min_size = 1,
    must_contain = ["extproj.sh"],
    target = ":external_archive",
)
