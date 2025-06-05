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

load("@bazel_skylib//:bzl_library.bzl", "bzl_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])  # Apache 2.0

filegroup(
    name = "all_files_for_testing",
    srcs = glob(["**"]) + [
        "//cc/private/rules_impl:srcs",
        "//cc/private/toolchain:srcs",
    ],
)

exports_files([
    "defs.bzl",
    "action_names.bzl",
    "system_library.bzl",
])

# The toolchain type used to distinguish cc toolchains.
alias(
    name = "toolchain_type",
    actual = "@bazel_tools//tools/cpp:toolchain_type",
)

filegroup(
    name = "action_names_test_files",
    testonly = True,
    srcs = [
        "BUILD",
        "action_names.bzl",
    ],
    visibility = ["//visibility:public"],
)

filegroup(
    name = "bzl_srcs",
    srcs = glob([
        "**/*.bzl",
    ]) + [
        "//cc/private/rules_impl:bzl_srcs",
        "//cc/private/toolchain:bzl_srcs",
    ],
    visibility = ["//visibility:public"],
)

filegroup(
    name = "srcs",
    srcs = glob([
        "**/*.bzl",
        "**/BUILD",
    ]) + [
        "//cc/private/rules_impl:srcs",
        "//cc/private/toolchain:srcs",
    ],
    visibility = ["//visibility:public"],
)

# TODO(aiuto): Find a way to strip this rule from the distribution tarball.
filegroup(
    name = "distribution",
    srcs = glob([
        "**",
    ]),
    visibility = [
        "//distro:__pkg__",
    ],
)

bzl_library(
    name = "find_cc_toolchain_bzl",
    srcs = ["find_cc_toolchain.bzl"],
    visibility = ["//visibility:public"],
)

bzl_library(
    name = "action_names_bzl",
    srcs = ["action_names.bzl"],
    visibility = ["//visibility:public"],
)

bzl_library(
    name = "cc_toolchain_config_lib_bzl",
    srcs = ["cc_toolchain_config_lib.bzl"],
    visibility = ["//cc/toolchains:__subpackages__"],
)

cc_toolchain_alias(name = "current_cc_toolchain")

# Use alias, because it doesn't build on Bazel 6.
alias(
    name = "optional_current_cc_toolchain",
    actual = "//cc/private/bazel7:optional_current_cc_toolchain",
)

cc_libc_top_alias(name = "current_libc_top")
