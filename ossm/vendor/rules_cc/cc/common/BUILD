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

bzl_library(
    name = "common",
    srcs = glob(["*.bzl"]),
    visibility = ["//visibility:public"],
    deps = ["//cc/private/rules_impl:native_bzl"],
)

bzl_library(
    name = "cc_helper_bzl",
    srcs = ["cc_helper.bzl"],
    visibility = ["//visibility:public"],
    deps = [":visibility_bzl"],
)

bzl_library(
    name = "cc_debug_helper_bzl",
    srcs = ["cc_debug_helper.bzl"],
    visibility = ["//visibility:public"],
    deps = [
        ":cc_helper_bzl",
        ":visibility_bzl",
        "//cc:find_cc_toolchain_bzl",
    ],
)

bzl_library(
    name = "visibility_bzl",
    srcs = ["visibility.bzl"],
    visibility = ["//visibility:private"],
)

filegroup(
    name = "srcs",
    srcs = glob([
        "**/*.bzl",
        "**/BUILD",
    ]),
    visibility = ["//visibility:public"],
)
