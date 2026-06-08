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

load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")
load("@rules_pkg//pkg:mappings.bzl", "pkg_attributes", "pkg_filegroup", "pkg_files", "pkg_mkdirs")
load("@rules_pkg//pkg:tar.bzl", "pkg_tar")

cc_library(
    name = "foolib",
    srcs = [
        "foocore.cc",
    ],
    hdrs = [
        "foo.h",
    ],
)

cc_binary(
    name = "food",
    srcs = [
        "food.cc",
    ],
    data = [
        ":foolib",
    ],
)

pkg_files(
    name = "binary",
    srcs = [
        ":food",
    ],
    attributes = pkg_attributes(
        group = "root",
        mode = "0551",
        owner = "root",
    ),
    prefix = "/usr/sbin",
)

pkg_tar(
    name = "mansrc",
    srcs = [
        "food.8",
    ],
    out = "food.8.gz",
    extension = ".gz",
)

pkg_files(
    name = "manpages",
    srcs = [
        ":mansrc",
    ],
    attributes = pkg_attributes(
        group = "man",
        mode = "0444",
        owner = "man",
    ),
    prefix = "man/man8",
    visibility = ["//visibility:public"],
)

pkg_mkdirs(
    name = "runtime",
    attributes = pkg_attributes(
        group = "bin",
        mode = "0771",
        owner = "foo",
    ),
    dirs = [
        "/var/tmp/foo",
        "/var/tmp/foo/queue",
    ],
)

pkg_filegroup(
    name = "arch",
    srcs = [
        ":binary",
        ":runtime",
    ],
    visibility = ["//visibility:public"],
)
