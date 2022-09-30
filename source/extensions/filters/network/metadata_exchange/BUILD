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

# Metadata Exchange filter

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_cc_test",
    "envoy_package",
)

envoy_package()

envoy_cc_library(
    name = "metadata_exchange",
    srcs = [
        "metadata_exchange.cc",
        "metadata_exchange_initial_header.cc",
    ],
    hdrs = [
        "metadata_exchange.h",
        "metadata_exchange_initial_header.h",
    ],
    repository = "@envoy",
    deps = [
        "//extensions/common:context",
        "//extensions/common:proto_util",
        "//source/extensions/filters/network/metadata_exchange/config:metadata_exchange_cc_proto",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/base:endian",
        "@com_google_absl//absl/strings",
        "@envoy//envoy/local_info:local_info_interface",
        "@envoy//envoy/network:connection_interface",
        "@envoy//envoy/network:filter_interface",
        "@envoy//envoy/runtime:runtime_interface",
        "@envoy//envoy/stats:stats_macros",
        "@envoy//envoy/stream_info:filter_state_interface",
        "@envoy//source/common/http:utility_lib",
        "@envoy//source/common/network:utility_lib",
        "@envoy//source/common/protobuf",
        "@envoy//source/common/protobuf:utility_lib",
        "@envoy//source/extensions/filters/common/expr:cel_state_lib",
        "@envoy//source/extensions/filters/network:well_known_names",
    ],
)

envoy_cc_library(
    name = "config_lib",
    srcs = ["config.cc"],
    hdrs = ["config.h"],
    repository = "@envoy",
    visibility = ["//visibility:public"],
    deps = [
        ":metadata_exchange",
        "//source/extensions/filters/network/metadata_exchange/config:metadata_exchange_cc_proto",
        "@envoy//envoy/registry",
        "@envoy//envoy/server:filter_config_interface",
    ],
)

envoy_cc_test(
    name = "metadataexchange_test",
    srcs = [
        "metadata_exchange_test.cc",
    ],
    repository = "@envoy",
    deps = [
        ":config_lib",
        ":metadata_exchange",
        "@envoy//source/common/protobuf",
        "@envoy//test/mocks/local_info:local_info_mocks",
        "@envoy//test/mocks/network:network_mocks",
        "@envoy//test/mocks/protobuf:protobuf_mocks",
        "@envoy//test/test_common:wasm_lib",
    ],
)
