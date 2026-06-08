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

load("@rules_python//python:defs.bzl", "py_binary", "py_library")

package(
    default_applicable_licenses = ["//:license"],
)

filegroup(
    name = "standard_package",
    srcs = [
        "BUILD",
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
    glob(["*.bzl"]),
    visibility = [
        "//distro:__pkg__",
        "//doc_build:__pkg__",
    ],
)

py_binary(
    name = "build_tar",
    srcs = ["build_tar.py"],
    imports = ["../../.."],
    python_version = "PY3",
    srcs_version = "PY3",
    visibility = ["//visibility:public"],
    deps = [
        ":tar_writer",
        "//pkg/private:archive",
        "//pkg/private:build_info",
        "//pkg/private:helpers",
        "//pkg/private:manifest",
    ],
)

py_library(
    name = "tar_writer",
    srcs = [
        "tar_writer.py",
    ],
    imports = ["../../.."],
    srcs_version = "PY3",
    visibility = [
        "//tests:__subpackages__",
    ],
)
