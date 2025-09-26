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
"""rules_pkg internal code.

All interfaces are subject to change at any time.
"""

load("@rules_python//python:defs.bzl", "py_library")

package(default_applicable_licenses = ["//:license"])

filegroup(
    name = "standard_package",
    srcs = [
        "BUILD",
        "install.py.tpl",
    ] + glob([
        "*.bzl",
        "*.py",
    ]),
    visibility = [
        "//distro:__pkg__",
        "//pkg:__pkg__",
    ],
)

exports_files(
    glob([
        "*.bzl",
    ]),
    visibility = [
        "//distro:__pkg__",
        "//doc_build:__pkg__",
        "//pkg:__pkg__",
    ],
)

# pkg_install's template script file
exports_files(
    ["install.py.tpl"],
    visibility = ["//visibility:public"],
)

config_setting(
    name = "private_stamp_detect",
    values = {"stamp": "1"},
    # When --incompatible_config_setting_private_default_visibility is set, this fails unless this is public.
    # TODO: refactor to clear up confusion that this is a "private" target with public access.
    visibility = ["//visibility:public"],
)

py_library(
    name = "build_info",
    srcs = [
        "build_info.py",
    ],
    imports = ["../.."],
    srcs_version = "PY3",
    visibility = [
        "//:__subpackages__",
        "//tests:__pkg__",
    ],
)

py_library(
    name = "archive",
    srcs = [
        "__init__.py",
        "archive.py",
    ],
    imports = ["../.."],
    srcs_version = "PY3",
    visibility = [
        "//:__subpackages__",
        "//tests:__subpackages__",
    ],
)

py_library(
    name = "helpers",
    srcs = [
        "__init__.py",
        "helpers.py",
    ],
    imports = ["../.."],
    srcs_version = "PY3",
    visibility = [
        "//:__subpackages__",
        "//tests:__pkg__",
    ],
)

py_library(
    name = "manifest",
    srcs = ["manifest.py"],
    imports = ["../.."],
    srcs_version = "PY3",
    visibility = ["//visibility:public"],
)
