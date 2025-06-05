# Copyright 2024 The Bazel Authors. All rights reserved.
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

load("@rules_pkg//pkg:mappings.bzl", "pkg_files")
load("@rules_pkg//pkg:rpm.bzl", "pkg_rpm", "pkg_sub_rpm")

pkg_files(
    name = "subrpm_files",
    srcs = [
        "BUILD",
    ],
)

pkg_sub_rpm(
    name = "subrpm",
    package_name = "subrpm",
    srcs = [
        ":subrpm_files",
    ],
    description = "Test subrpm description",
    provides = [
        "someprovision",
    ],
    requires = [
        "somerpm",
    ],
    summary = "Test subrpm",
)

pkg_files(
    name = "rpm_files",
    srcs = [
        "MODULE.bazel",
        "README.md",
    ],
)

pkg_rpm(
    name = "test-rpm",
    srcs = [
        ":rpm_files",
    ],
    architecture = "x86_64",
    description = "This is a package description.",
    license = "Apache License, v2.0",
    provides = [
        "somefile",
    ],
    release = "0",
    requires = [
        "somerpm",
    ],
    subrpms = [
        ":subrpm",
    ],
    summary = "rules_pkg example RPM",
    version = "1",
)

# If you have rpmbuild, you probably have rpm2cpio too.
# Feature idea: Add rpm2cpio and cpio to the rpmbuild toolchain
genrule(
    name = "inspect_content",
    srcs = [":test-rpm"],
    outs = ["content.txt"],
    cmd = "rpm2cpio $(locations :test-rpm) | cpio -ivt >$@",
)
