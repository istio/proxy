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

load("@rules_pkg//pkg:mappings.bzl", "pkg_attributes", "pkg_filegroup", "pkg_files", "strip_prefix")
load("@rules_pkg//pkg:tar.bzl", "pkg_tar")
load("//:foo_defs.bzl", "shared_object_path_selector")

# There are localized message catalogs for all the components.

# Typically, you would have a lot of these and drop them in, so using a glob
# to gather them is appropriate.
pkg_files(
    name = "messages",
    srcs = glob(["**/msg.cat"]),
    attributes = pkg_attributes(
        # We know they should be read only. We don't know who should own them.
        mode = "0444",
    ),
    strip_prefix = strip_prefix.from_pkg(),
)

# Use the English catalog again under a different name.
# This is a trick for reusing a single source file or files in multiple places
# in the output tree. Here we place foo/en/msg.cat under foo/en_GB/msg.cat.
# This is more efficient than having a rule copy the files to a new location.
# The final package builder is passed just the information that the original
# srcs should be placed in the output archive under this other name.
pkg_files(
    name = "extra_messages",
    srcs = ["foo/en/msg.cat"],
    attributes = pkg_attributes(
        mode = "0444",
    ),
    prefix = "foo/en_GB",
    strip_prefix = strip_prefix.from_pkg(),
)

# This gathers together all our message catalogs, and adds a prefix based
# on the target OS
pkg_filegroup(
    name = "all",
    srcs = [
        ":extra_messages",
        ":messages",
    ],
    prefix = select(shared_object_path_selector) + "/locale",
    visibility = ["//visibility:public"],
)

pkg_files(
    name = "mraw",
    srcs = glob(["**/msg.cat"]),
    attributes = pkg_attributes(
        mode = "0444",
    ),
    strip_prefix = strip_prefix.from_root("resources"),
)

pkg_tar(
    name = "raw",
    srcs = [":mraw"],
)
