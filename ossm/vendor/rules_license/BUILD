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

load("@rules_license//:version.bzl", "version")
load("@rules_license//rules:license.bzl", "license")
load("@rules_license//rules:package_info.bzl", "package_info")

package(
    default_applicable_licenses = [":license", ":package_info"],
    default_visibility = ["//visibility:public"],
)

licenses(["notice"])

license(
    name = "license",
    license_kinds = [
        "@rules_license//licenses/spdx:Apache-2.0",
    ],
    license_text = "LICENSE",
)

package_info(
    name = "package_info",
    package_name = "rules_license",
    package_version = version,
)

exports_files(
    ["LICENSE", "WORKSPACE"],
    visibility = ["//visibility:public"],
)

exports_files(
    glob([
        "*.bzl",
    ]),
    visibility = ["//visibility:public"],
)

filegroup(
    name = "standard_package",
    srcs = glob([
        "*.bzl",
        "*.md",
    ]) + [
        "BUILD",
        "LICENSE",
        "MODULE.bazel",
        "WORKSPACE.bzlmod",
    ],
    visibility = ["//distro:__pkg__"],
)

filegroup(
    name = "docs_deps",
    srcs = [
        ":standard_package",
        "//rules:standard_package",
        "//rules_gathering:standard_package",
    ],
    visibility = ["//visibility:public"],
)
