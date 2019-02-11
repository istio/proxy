# Copyright 2019 Istio Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0

# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

licenses(["notice"])

cc_library(
    name = "dynamic_encoding",
    srcs = [
        "message_encoder.cc",
        "message_encoder_builder.cc",
        "primitive_encoder.cc",
        "resolver.cc",
        "util.cc",
    ],
    hdrs = [
        "compiler.h",
        "encoder.h",
        "evaluator.h",
        "istio_message.h",
        "message_encoder.h",
        "message_encoder_builder.h",
        "primitive_encoder.h",
        "resolver.h",
        "util.h",
    ],
    visibility = [":__subpackages__"],
    deps = [
        "//external:protobuf",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/types:any",
        "@mixerapi_git//:policy_v1beta1_cc_proto",
    ],
)

cc_test(
    name = "static_encoder_test",
    size = "small",
    srcs = [
        "static_encoder_test.cc",
    ],
    data = ["//src/istio/dynamic_encoding/testdata:types.descriptor"],
    linkstatic = 1,
    deps = [
        ":dynamic_encoding",
        "//external:googletest_main",
        "//external:protobuf",
        "//src/istio/dynamic_encoding/testdata:types",
        "@bazel_tools//tools/cpp/runfiles",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:optional",
    ],
)

cc_test(
    name = "dynamic_encoder_test",
    size = "small",
    srcs = [
        "dynamic_encoder_test.cc",
    ],
    data = ["//src/istio/dynamic_encoding/testdata:types.descriptor"],
    linkstatic = 1,
    deps = [
        ":dynamic_encoding",
        "//external:googletest_main",
        "//external:protobuf",
        "//src/istio/dynamic_encoding/testdata:types",
        "@bazel_tools//tools/cpp/runfiles",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:optional",
    ],
)
