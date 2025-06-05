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
    name = "toolchain_rules",
    srcs = glob(["*.bzl"]),
    visibility = ["//visibility:public"],
    deps = [
        "//cc:action_names_bzl",
        "//cc:cc_toolchain_config_lib_bzl",
        "//cc:find_cc_toolchain_bzl",
        "//cc/private/rules_impl:cc_flags_supplier_lib_bzl",
        "//cc/private/rules_impl:native_bzl",
        "//cc/toolchains/impl:toolchain_impl_rules",
        "@bazel_skylib//rules/directory:glob",
    ],
)

filegroup(
    name = "srcs",
    srcs = glob([
        "**/*.bzl",
        "**/BUILD",
    ]),
    visibility = ["//visibility:public"],
)
