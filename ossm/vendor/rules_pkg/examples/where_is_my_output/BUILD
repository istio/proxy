# -*- coding: utf-8 -*-
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
# Build a trivial .deb package to show how to find the output files.

load("@rules_pkg//pkg:deb.bzl", "pkg_deb")
load("@rules_pkg//pkg:tar.bzl", "pkg_tar")

genrule(
    name = "generate_files",
    outs = [
        "etc/example.conf",
        "usr/bin/a_binary",
    ],
    cmd = "for i in $(OUTS); do echo 1 >$$i; done",
)

pkg_tar(
    name = "content",
    srcs = [":generate_files"],
)

pkg_deb(
    name = "deb",
    data = ":content",
    description = "My wonderful product",
    maintainer = "someone@somewhere.com",
    package = "mwp",
    version = "3.14",
)

# We can also depend just on the .changes file

filegroup(
    name = "the_changes_file",
    srcs = [":deb"],
    output_group = "changes",
)

genrule(
    name = "use_changes_file",
    srcs = [":the_changes_file"],
    outs = ["copy_of_changes.txt"],
    cmd = "cp $(location :the_changes_file) $@",
)
