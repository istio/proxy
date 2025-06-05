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
"""toolchain to wrap the git binary.

Type: @rules_pkg//toolchains/git:git_toolchain_type

Toolchains:
- git_missing_toolchain: provides a fallback toolchain for exec platforms
  where git might not be available.

- git_auto_toolchain: a toolchain that uses the installed git. See
  git_configure.bzl%find_system_git for usage.
"""

load("//toolchains/git:git.bzl", "git_toolchain", "is_git_available")

package(default_applicable_licenses = ["//:license"])

filegroup(
    name = "standard_package",
    srcs = glob(["*"]),
    visibility = ["//distro:__pkg__"],
)

exports_files(
    glob(["*"]),
    visibility = ["//visibility:public"],
)

# Expose the availability of an actual git as a config_setting, so we can
# select() on it.
config_setting(
    name = "have_git",
    flag_values = {
        ":is_git_available": "1",
    },
    visibility = ["//visibility:public"],
)

# Expose the availability of an actual git as a feature flag, so we can
# create a config_setting from it.
is_git_available(
    name = "is_git_available",
    visibility = ["//:__subpackages__"],
)

toolchain_type(
    name = "git_toolchain_type",
    visibility = ["//visibility:public"],
)

# git_missing_toolchain provides a fallback toolchain so that toolchain
# resolution can succeed even on platforms that do not have a working git.
# If this toolchain is selected, the constraint ":have_git" will not be
# satistifed.
git_toolchain(
    name = "no_git",
)

toolchain(
    name = "git_missing_toolchain",
    toolchain = ":no_git",
    toolchain_type = ":git_toolchain_type",
)
