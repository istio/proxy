# Copyright 2022 The Bazel Authors. All rights reserved.
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

"""License declaration and compliance checking tools."""

load("@rules_python//python:defs.bzl", "py_binary", "py_library")

package(
    default_applicable_licenses = ["//:license", "//:package_info"],
    default_visibility = ["//visibility:public"],
)

licenses(["notice"])

filegroup(
    name = "standard_package",
    srcs = glob(["**"]),
    visibility = ["//distro:__pkg__"],
)

exports_files(["diff_test.sh"])

py_binary(
    name = "checker_demo",
    srcs = ["checker_demo.py"],
    python_version = "PY3",
    visibility = ["//visibility:public"],
)

py_library(
    name = "sbom_lib",
    srcs = ["sbom.py"],
    visibility = ["//visibility:public"],
)

py_binary(
    name = "write_sbom",
    srcs = ["write_sbom.py"],
    deps = [":sbom_lib"],
    python_version = "PY3",
    visibility = ["//visibility:public"],
)

py_binary(
    name = "write_workspace_sbom",
    srcs = ["write_workspace_sbom.py"],
    deps = [":sbom_lib"],
    python_version = "PY3",
    visibility = ["//visibility:public"],
)
