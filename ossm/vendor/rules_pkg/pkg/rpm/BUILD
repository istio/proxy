# Copyright 2020 The Bazel Authors. All rights reserved.
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

load("@rules_python//python:defs.bzl", "py_binary")

package(default_applicable_licenses = ["//:license"])

exports_files(
    glob([
        "*.tpl",
    ]),
    visibility = ["//visibility:public"],
)

filegroup(
    name = "standard_package",
    srcs = glob([
        "*.py",
        "*.tpl",
    ]) + [
        "BUILD",
    ],
    visibility = ["//pkg:__pkg__"],
)

# Helper script used to augment the %install scriptlet and %files list with
# those found in TreeArtifacts (directory outputs) See also #292.
py_binary(
    name = "augment_rpm_files_install",
    srcs = ["augment_rpm_files_install.py"],
    visibility = ["//visibility:public"],
)
