# Copyright 2016 Google Inc. All Rights Reserved.
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
load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library", "cc_test")
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@io_bazel_rules_docker//cc:image.bzl", "cc_image")
load("@io_bazel_rules_docker//container:container.bzl", "container_push")

cc_proto_library(
    name = "benchmark_cc_proto",
    testonly = 1,
    deps = [":benchmark_proto"],
)

cc_proto_library(
    name = "com_google_protobuf_struct_cc_proto",
    testonly = 1,
    deps = ["@com_google_protobuf//:struct_proto"],
)

proto_library(
    name = "benchmark_proto",
    testonly = 1,
    srcs = ["benchmark.proto"],
    deps = [
        "@com_google_googleapis//google/api:annotations_proto",
        "@com_google_protobuf//:struct_proto",
    ],
)

cc_binary(
    name = "benchmark_main",
    testonly = 1,
    srcs = ["benchmark_main.cc"],
    data = [
        "benchmark_service.textproto",
    ],
    deps = [
        "benchmark_cc_proto",
        ":benchmark_input_stream",
        ":utils",
        "//src:json_request_translator",
        "//src:response_to_json_translator",
        "//src:type_helper",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/random",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_benchmark//:benchmark",
        "@com_google_benchmark//:benchmark_main",
        "@com_google_googleapis//google/api:service_cc_proto",
    ],
)

# Note: cc_image rule won't build the binary inside the container. It builds the
# binary locally and copies the binary into the container.
# This cc_image rule with v.0.25.0 rules_docker release uses Debian 11 as base.
# Please make sure if you're building from Debian 11 system as well.
cc_image(
    name = "benchmark_main_image",
    testonly = 1,
    binary = ":benchmark_main",
)

# Example run command:
# bazel run //perf_benchmark:benchmark_main_image_push \
# --define=PUSH_REGISTRY=gcr.io \
# --define=PUSH_PROJECT=project-id \
# --define=PUSH_TAG=latest
container_push(
    name = "benchmark_main_image_push",
    testonly = 1,
    format = "Docker",
    image = ":benchmark_main_image",
    registry = "$(PUSH_REGISTRY)",
    repository = "$(PUSH_PROJECT)/grpc-httpjson-transcoding-benchmark",
    tag = "$(PUSH_TAG)",
)

cc_library(
    name = "utils",
    testonly = 1,
    srcs = ["utils.cc"],
    hdrs = ["utils.h"],
    deps = [
        "benchmark_cc_proto",
        "//src:type_helper",
        "//test:test_common",
        "@com_github_nlohmann_json//:json",
        "@com_google_absl//absl/random",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_googleapis//google/api:service_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "benchmark_input_stream",
    testonly = 1,
    srcs = ["benchmark_input_stream.cc"],
    hdrs = ["benchmark_input_stream.h"],
    deps = [
        "//src:transcoder_input_stream",
        "@com_google_absl//absl/log:absl_check",
        "@com_google_absl//absl/log:absl_log",
        "@com_google_absl//absl/strings",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "utils_test",
    srcs = [
        "utils_test.cc",
    ],
    deps = [
        ":utils",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "benchmark_input_stream_test",
    srcs = [
        "benchmark_input_stream_test.cc",
    ],
    data = [
        "benchmark_service.textproto",
    ],
    deps = [
        ":benchmark_input_stream",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "benchmark_input_stream_translator_integration_test",
    srcs = [
        "benchmark_input_stream_translator_integration_test.cc",
    ],
    data = [
        "benchmark_service.textproto",
    ],
    deps = [
        "com_google_protobuf_struct_cc_proto",
        ":benchmark_cc_proto",
        ":benchmark_input_stream",
        ":utils",
        "//src:json_request_translator",
        "//src:response_to_json_translator",
        "//src:type_helper",
        "@com_github_nlohmann_json//:json",
        "@com_google_absl//absl/log:absl_check",
        "@com_google_googleapis//google/api:service_cc_proto",
        "@com_google_googletest//:gtest_main",
    ],
)
