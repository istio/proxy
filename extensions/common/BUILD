# Copyright 2019 Istio Authors. All Rights Reserved.
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
#
################################################################################
#

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_cc_test",
)
load(
    "@com_github_google_flatbuffers//:build_defs.bzl",
    "DEFAULT_FLATC_ARGS",
    "flatbuffer_library_public",
)
load(
    "@envoy//test/extensions:extensions_build_system.bzl",
    "envoy_extension_cc_benchmark_binary",
)

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

envoy_cc_library(
    name = "proto_util",
    srcs = [
        "proto_util.cc",
    ],
    hdrs = [
        "proto_util.h",
    ],
    repository = "@envoy",
    deps = [
        ":metadata_object_lib",
        ":node_info_fb_cc",
        ":util",
        "@com_google_protobuf//:protobuf",
        "@proxy_wasm_cpp_host//:null_lib",
    ],
)

envoy_cc_library(
    name = "util",
    srcs = [
        "util.cc",
    ],
    hdrs = [
        "util.h",
    ],
    repository = "@envoy",
    deps = [
        "@proxy_wasm_cpp_host//:null_lib",
    ],
)

envoy_cc_test(
    name = "proto_util_test",
    size = "small",
    srcs = ["proto_util_test.cc"],
    repository = "@envoy",
    deps = [
        ":node_info_fb_cc",
        ":proto_util",
        "@com_google_protobuf//:protobuf",
    ],
)

envoy_cc_test(
    name = "util_test",
    size = "small",
    srcs = ["util_test.cc"],
    repository = "@envoy",
    deps = [
        ":util",
    ],
)

# TODO: fix this build on mac m1?
envoy_extension_cc_benchmark_binary(
    name = "proto_util_speed_test",
    srcs = select({
        "@envoy//bazel:linux": ["proto_util_speed_test.cc"],
        "//conditions:default": [],
    }),
    extension_names = ["envoy.wasm.runtime.null"],
    external_deps = [
        "benchmark",
    ],
    repository = "@envoy",
    deps = select({
        "@envoy//bazel:linux": [
            ":metadata_object_lib",
            ":node_info_fb_cc",
            ":proto_util",
            "@envoy//source/common/stream_info:filter_state_lib",
            "@envoy//source/extensions/filters/common/expr:cel_state_lib",
            "@envoy//test/test_common:status_utility_lib",
        ],
        "//conditions:default": [],
    }),
)

flatbuffer_library_public(
    name = "node_info_fbs",
    srcs = ["node_info.fbs"],
    outs = [
        "node_info_bfbs_generated.h",
        "node_info_generated.h",
    ],
    flatc_args = DEFAULT_FLATC_ARGS + ["--bfbs-gen-embed"],
    language_flag = "-c",
)

cc_library(
    name = "node_info_fb_cc",
    srcs = [":node_info_fbs"],
    hdrs = [":node_info_fbs"],
    features = ["-parse_headers"],
    linkstatic = True,
    deps = [
        "@com_github_google_flatbuffers//:flatbuffers",
        "@com_github_google_flatbuffers//:runtime_cc",
    ],
)

envoy_cc_library(
    name = "metadata_object_lib",
    srcs = ["metadata_object.cc"],
    hdrs = ["metadata_object.h"],
    repository = "@envoy",
    deps = [
        ":node_info_fb_cc",
        "@com_github_google_flatbuffers//:flatbuffers",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:optional",
        "@envoy//envoy/common:hashable_interface",
        "@envoy//envoy/network:filter_interface",
        "@envoy//envoy/ssl:connection_interface",
        "@envoy//envoy/stream_info:filter_state_interface",
        "@envoy//source/common/common:hash_lib",
    ],
)

envoy_cc_test(
    name = "metadata_object_test",
    srcs = ["metadata_object_test.cc"],
    repository = "@envoy",
    deps = [
        ":metadata_object_lib",
        "@envoy//test/mocks/ssl:ssl_mocks",
    ],
)
