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

"""toolchain to wrap an rpmbuild binary.

Type: @rules_pkg//toolchains/rpm:rpmbuild_toolchain_type

Toolchains:
- rpmbuild_missing_toolchain: provides a fallback toolchain for exec platforms
  where rpmbuild might not be available.

- rpmbuild_auto_toolchain: a toolchain that uses the installed rpmbuild. See
  rpmbuild_configure.bzl%find_system_rpmbuild for usage.
"""

load("//toolchains/rpm:rpmbuild.bzl", "is_rpmbuild_available", "rpmbuild_toolchain")

package(default_applicable_licenses = ["//:license"])

filegroup(
    name = "standard_package",
    srcs = glob([
        "*",
    ]),
    visibility = [
        "//distro:__pkg__",
        "//doc_build:__pkg__",
    ],
)

exports_files(
    glob(["*"]),
    visibility = ["//visibility:public"],
)

# Expose the availability of an actual rpmbuild as a config_setting, so we can
# select() on it.
config_setting(
    name = "have_rpmbuild",
    flag_values = {
        ":is_rpmbuild_available": "1",
    },
    visibility = ["//visibility:public"],
)

# Expose the availability of an actual rpmbuild as a feature flag, so we can
# create a config_setting from it.
is_rpmbuild_available(
    name = "is_rpmbuild_available",
    visibility = ["//:__subpackages__"],
)

toolchain_type(
    name = "rpmbuild_toolchain_type",
    visibility = ["//visibility:public"],
)

# rpmbuild_missing_toolchain provides a fallback toolchain so that toolchain
# resolution can succeed even on platforms that do not have a working rpmbuild.
# If this toolchain is selected, the constraint ":have_rpmbuild" will not be
# satistifed.
rpmbuild_toolchain(
    name = "no_rpmbuild",
)

toolchain(
    name = "rpmbuild_missing_toolchain",
    toolchain = ":no_rpmbuild",
    toolchain_type = ":rpmbuild_toolchain_type",
)
