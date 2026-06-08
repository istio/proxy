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
load("//cc:starlark_doc_extract_helper.bzl", "starlark_doc_extract_helper")

bzl_library(
    name = "common",
    srcs = [
        "cc_common.bzl",
        "cc_helper.bzl",
        "cc_helper_internal.bzl",
        "cc_info.bzl",
        "cc_shared_library_hint_info.bzl",
        "debug_package_info.bzl",
        "objc_info.bzl",
        "semantics.bzl",
        "visibility.bzl",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//cc/private:cc_internal_bzl",
        "//cc/private:paths_bzl",
        "//cc/private/rules_impl:native_cc_common_bzl",
        "@bazel_skylib//lib:paths",
        "@cc_compatibility_proxy//:symbols_bzl",
    ],
)

bzl_library(
    name = "cc_helper_bzl",
    srcs = ["cc_helper.bzl"],
    visibility = ["//visibility:public"],
    deps = [
        ":cc_helper_internal_bzl",
        ":common",
        ":visibility_bzl",
        "//cc:find_cc_toolchain_bzl",
        "//cc/private:paths_bzl",
        "//cc/private/rules_impl:objc_common",
    ],
)

bzl_library(
    name = "cc_shared_library_info_bzl",
    srcs = ["cc_shared_library_info.bzl"],
    visibility = ["//cc:__subpackages__"],
    deps = [
        "@cc_compatibility_proxy//:symbols_bzl",
    ],
)

bzl_library(
    name = "semantics_bzl",
    srcs = ["semantics.bzl"],
    visibility = ["//cc:__subpackages__"],
)

bzl_library(
    name = "cc_helper_internal_bzl",
    srcs = ["cc_helper_internal.bzl"],
    visibility = ["//cc:__subpackages__"],
    deps = [
        "//cc/private:cc_internal_bzl",
        "//cc/private:paths_bzl",
        "@bazel_skylib//lib:paths",
    ],
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

# unused targets to test that we have the right bzl_library graph
[starlark_doc_extract_helper(
    name = f + "_doc_extract",
    src = f,
    visibility = ["//visibility:private"],
    deps = [
        ":common",
    ],
) for f in glob(
    ["*.bzl"],
    exclude = [
        "cc_helper.bzl",
        "cc_debug_helper.bzl",
        "visibility.bzl",
        "semantics.bzl",
        "cc_helper_internal.bzl",
        "cc_shared_library_info.bzl",
    ],
)]
