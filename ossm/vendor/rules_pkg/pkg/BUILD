# Copyright 2019 The Bazel Authors. All rights reserved.
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

load("@rules_pkg//pkg/private:make_starlark_library.bzl", "starlark_library")

# -*- coding: utf-8 -*-
load("@rules_python//python:defs.bzl", "py_binary", "py_library")

package(default_applicable_licenses = ["//:license"])

exports_files(
    glob([
        "*.bzl",
    ]),
    visibility = ["//visibility:public"],
)

constraint_setting(name = "not_compatible_setting")

constraint_value(
    name = "not_compatible",
    constraint_setting = ":not_compatible_setting",
    visibility = ["//visibility:public"],
)

filegroup(
    name = "standard_package",
    srcs = glob([
        "*.bzl",
        "*.py",
        "*.tpl",
    ]) + [
        "BUILD",
        "//pkg/legacy:standard_package",
        "//pkg/rpm:standard_package",
    ],
    visibility = [
        "//distro:__pkg__",
        "//pkg:__pkg__",
    ],
)

starlark_library(
    name = "bzl_srcs",
    srcs = [
        ":standard_package",
        "//pkg:pkg.bzl",
        "//pkg:rpm_pfg.bzl",
        "//pkg/private:standard_package",
        "//pkg/private/deb:standard_package",
        "//pkg/private/tar:standard_package",
        "//pkg/private/zip:standard_package",
        "//pkg/releasing:standard_package",
    ],
    visibility = ["//visibility:public"],
)

# Used by pkg_rpm in rpm.bzl.
py_binary(
    name = "make_rpm",
    srcs = ["make_rpm.py"],
    imports = [".."],
    python_version = "PY3",
    srcs_version = "PY3",
    target_compatible_with = select({
        "//toolchains/rpm:have_rpmbuild": [],
        "//conditions:default": [":not_compatible"],
    }),
    visibility = ["//visibility:public"],
    deps = [
        "//pkg:make_rpm_lib",
        "//pkg/private:archive",
    ],
)

py_library(
    name = "make_rpm_lib",
    srcs = ["make_rpm.py"],
    imports = [".."],
    srcs_version = "PY3",
    visibility = [
        "//experimental:__subpackages__",
        "//tests:__subpackages__",
    ],
    deps = [
        "//pkg/private:archive",
        "//pkg/private:helpers",
    ],
)

py_binary(
    name = "filter_directory",
    srcs = ["filter_directory.py"],
    python_version = "PY3",
    visibility = ["//visibility:public"],
)

py_library(
    name = "filter_directory_lib",
    srcs = ["filter_directory.py"],
    srcs_version = "PY3",
    visibility = ["//visibility:public"],
)

exports_files(["verify_archive_test_main.py.tpl"])
