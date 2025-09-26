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
# -*- coding: utf-8 -*-

load("@bazel_skylib//rules:copy_file.bzl", "copy_file")
load("@rules_python//python:defs.bzl", "py_library", "py_test")
load("//pkg:mappings.bzl", "pkg_attributes", "pkg_mkdirs", "pkg_mklink")
load("//pkg:zip.bzl", "pkg_zip")
load("//tests:my_package_name.bzl", "my_package_naming")
load("//tests/util:defs.bzl", "directory")

package(default_applicable_licenses = ["//:license"])

py_library(
    name = "zip_test_lib",
    srcs = [
        "zip_test_lib.py",
    ],
    imports = ["../.."],
    srcs_version = "PY3",
    deps = [
        "@rules_python//python/runfiles",
    ],
)

genrule(
    name = "generate_files",
    outs = [
        "etc/nsswitch.conf",
        "usr/titi",
    ],
    cmd = "for i in $(OUTS); do echo 1 >$$i; done",
)

pkg_mkdirs(
    name = "dirs",
    attributes = pkg_attributes(
        group = "bar",
        mode = "711",
        user = "foo",
    ),
    dirs = [
        "foodir",
    ],
)

pkg_mklink(
    name = "link",
    attributes = pkg_attributes(
        mode = "555",
    ),
    link_name = "/usr/bin/foo",
    target = "/usr/local/foo/foo.real",
)

directory(
    name = "generate_tree",
    contents = "hello there",
    filenames = [
        # buildifier: don't sort
        "b/e",
        "a/a",
        "b/c/d",
        "b/d",
        "a/b/c",
    ],
)

copy_file(
    name = "zipcontent_loremipsum",
    src = "//tests:testdata/loremipsum.txt",
    out = "zipcontent/loremipsum.txt",
)

pkg_zip(
    name = "test_zip_empty",
    srcs = [],
)

# The contents of this file should equal test_zip_empty
pkg_zip(
    name = "test_zip_basic_renamed",
    srcs = [
        ":dirs",
        ":link",
        "//tests:an_executable",
        "//tests:testdata/hello.txt",
        "//tests:testdata/loremipsum.txt",
    ],
    package_file_name = "test_zip_basic_renamed.foo",
)

pkg_zip(
    name = "test_zip_basic",
    srcs = [
        ":dirs",
        ":link",
        "//tests:an_executable",
        "//tests:testdata/hello.txt",
        "//tests:testdata/loremipsum.txt",
    ],
)

pkg_zip(
    name = "test_zip_out",
    srcs = [
        "//tests:testdata/hello.txt",
        "//tests:testdata/loremipsum.txt",
    ],
    out = "test_zip_out.foo",
)

pkg_zip(
    name = "test_zip_timestamp",
    srcs = [
        "//tests:testdata/hello.txt",
    ],
    timestamp = 1234567890,
)

pkg_zip(
    name = "test_zip_tree",
    srcs = [":generate_tree"],
)

pkg_zip(
    name = "test_zip_permissions",
    srcs = [
        "//tests:testdata/executable.sh",
    ],
    mode = "644",
    timestamp = 1234567890,
)

# This should be equal to test_zip_basic because timestamps are rounded
# up to Jan 1 1980.
pkg_zip(
    name = "test_zip_basic_timestamp_before_epoch",
    srcs = [
        ":dirs",
        ":link",
        "//tests:an_executable",
        "//tests:testdata/hello.txt",
        "//tests:testdata/loremipsum.txt",
    ],
    timestamp = 0,
)

pkg_zip(
    name = "test-zip-strip_prefix-empty",
    srcs = [
        "zipcontent_loremipsum",
    ],
    strip_prefix = "",
)

pkg_zip(
    name = "test-zip-strip_prefix-none",
    srcs = [
        "zipcontent_loremipsum",
    ],
)

pkg_zip(
    name = "test-zip-strip_prefix-zipcontent",
    srcs = [
        "zipcontent_loremipsum",
    ],
    strip_prefix = "zipcontent",
)

pkg_zip(
    name = "test-zip-strip_prefix-dot",
    srcs = [
        "zipcontent_loremipsum",
    ],
    strip_prefix = ".",
)

filegroup(
    name = "test_zip_strip_prefix",
    srcs = [
        "test-zip-strip_prefix-dot",
        "test-zip-strip_prefix-empty",
        "test-zip-strip_prefix-none",
        "test-zip-strip_prefix-zipcontent",
    ],
)

pkg_mklink(
    name = "mylink",
    link_name = "mylink",
    target = "dangling",
)

# All of these should produce the same zip file.
[pkg_zip(
    name = "test_zip_package_dir" + str(idx),
    srcs = [
        ":mylink",
        "//tests:testdata/hello.txt",
        "//tests:testdata/loremipsum.txt",
    ],
    package_dir = package_dir,
) for (idx, package_dir) in enumerate([
    "abc/def",
    "/abc/def",
    "/abc/def/",
])]

my_package_naming(
    name = "my_package_variables",
    label = "some_value",
)

pkg_zip(
    name = "test_zip_package_dir_substitution",
    srcs = [
        "//tests:testdata/hello.txt",
        "//tests:testdata/loremipsum.txt",
    ],
    package_dir = "/level1/{label}/level3",
    package_variables = ":my_package_variables",
)

# Different compressions
pkg_zip(
    name = "test_zip_deflated_level_3",
    srcs = [
        "//tests:testdata/loremipsum.txt",
    ],
    compression_level = 3,
    compression_type = "deflated",
)

pkg_zip(
    name = "test_zip_bzip2",
    srcs = [
        "//tests:testdata/loremipsum.txt",
    ],
    compression_type = "bzip2",
)

pkg_zip(
    name = "test_zip_lzma",
    srcs = [
        "//tests:testdata/loremipsum.txt",
    ],
    compression_type = "lzma",
)

pkg_zip(
    name = "test_zip_stored",
    srcs = [
        "//tests:testdata/loremipsum.txt",
    ],
    compression_type = "stored",
)

py_test(
    name = "zip_test",
    srcs = [
        "zip_test.py",
    ],
    data = [
        ":test-zip-strip_prefix-dot.zip",
        ":test-zip-strip_prefix-empty.zip",
        ":test-zip-strip_prefix-none.zip",
        ":test-zip-strip_prefix-zipcontent.zip",
        ":test_zip_basic.zip",
        ":test_zip_bzip2",
        ":test_zip_deflated_level_3",
        ":test_zip_empty.zip",
        ":test_zip_lzma",
        ":test_zip_package_dir0.zip",
        ":test_zip_package_dir_substitution.zip",
        ":test_zip_permissions.zip",
        ":test_zip_stored",
        ":test_zip_timestamp.zip",
        ":test_zip_tree.zip",
    ],
    python_version = "PY3",
    deps = [
        ":zip_test_lib",
    ],
)

py_test(
    name = "zip_byte_for_byte_test",
    srcs = [
        "zip_byte_for_byte_test.py",
    ],
    data = [
        ":test_zip_basic.zip",
        ":test_zip_basic_renamed",
        # We do not actually use this file, we just want to make sure we build
        # it with this name.
        ":test_zip_out.foo",
        ":test_zip_package_dir0.zip",
        # these could be replaced with diff_test() rules (from skylib)
        ":test_zip_basic_timestamp_before_epoch.zip",
        ":test_zip_package_dir1.zip",
        ":test_zip_package_dir2.zip",
    ],
    python_version = "PY3",
    deps = [
        ":zip_test_lib",
    ],
)

pkg_zip(
    name = "unicode_names",
    srcs = ["//tests:utf8_files"],
)

py_test(
    name = "unicode_test",
    srcs = [
        "unicode_test.py",
    ],
    data = [
        ":unicode_names",
    ],
    python_version = "PY3",
    deps = [
        ":zip_test_lib",
    ],
)
