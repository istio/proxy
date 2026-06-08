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

load("@bazel_skylib//:bzl_library.bzl", "bzl_library")

filegroup(
    name = "bzl_srcs",
    srcs = glob([
        "**/*.bzl",
    ]) + [
        "//cc/private/compile:bzl_srcs",
        "//cc/private/link:bzl_srcs",
        "//cc/private/rules_impl:bzl_srcs",
        "//cc/private/toolchain:bzl_srcs",
        "//cc/private/toolchain_config:bzl_srcs",
    ],
    visibility = ["//visibility:public"],
)

filegroup(
    name = "srcs",
    srcs = glob([
        "**/BUILD",
        "*.bzl",
    ]) + [
        "//cc/private/compile:srcs",
        "//cc/private/link:srcs",
        "//cc/private/rules_impl:srcs",
        "//cc/private/toolchain:srcs",
        "//cc/private/toolchain_config:srcs",
    ],
    visibility = ["//visibility:public"],
)

bzl_library(
    name = "paths_bzl",
    srcs = ["paths.bzl"],
    visibility = ["//cc:__subpackages__"],
)

bzl_library(
    name = "cc_common",
    srcs = ["cc_common.bzl"],
    visibility = [
        "//cc:__subpackages__",
        "@cc_compatibility_proxy//:__pkg__",
    ],
    deps = [
        ":cc_info_bzl",
        ":cc_internal_bzl",
        ":cc_launcher_info_bzl",
        ":cc_shared_library_hint_info_bzl",
        ":debug_package_info_bzl",
        ":objc_info_bzl",
        "//cc/private/compile:compile_bzl",
        "//cc/private/link:link_bzl",
        "//cc/private/rules_impl:cc_toolchain_info_bzl",
        "//cc/private/rules_impl:native_cc_common_bzl",
        "//cc/private/toolchain_config:toolchain_config_bzl",
    ],
)

bzl_library(
    name = "cc_info_bzl",
    srcs = ["cc_info.bzl"],
    visibility = ["//cc:__subpackages__"],
)

bzl_library(
    name = "debug_package_info_bzl",
    srcs = ["debug_package_info.bzl"],
    visibility = ["//cc:__subpackages__"],
)

bzl_library(
    name = "cc_internal_bzl",
    srcs = ["cc_internal.bzl"],
    visibility = ["//cc:__subpackages__"],
)

bzl_library(
    name = "cc_shared_library_info_bzl",
    srcs = ["cc_shared_library_info.bzl"],
    visibility = [
        "//cc:__subpackages__",
        "@cc_compatibility_proxy//:__pkg__",
    ],
)

bzl_library(
    name = "cc_shared_library_hint_info_bzl",
    srcs = ["cc_shared_library_hint_info.bzl"],
    visibility = ["//cc:__subpackages__"],
)

bzl_library(
    name = "cc_launcher_info_bzl",
    srcs = ["cc_launcher_info.bzl"],
    visibility = ["//cc:__subpackages__"],
    deps = ["//cc/common:cc_helper_internal_bzl"],
)

bzl_library(
    name = "objc_info_bzl",
    srcs = ["objc_info.bzl"],
    visibility = ["//cc:__subpackages__"],
)
