# Copyright Istio Authors. All Rights Reserved.
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

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_extension",
    "envoy_cc_library",
    "envoy_cc_test",
    "envoy_extension_package",
)

licenses(["notice"])  # Apache 2

envoy_extension_package()

envoy_cc_library(
    name = "metadata_to_peer_node_lib",
    srcs = ["metadata_to_peer_node.cc"],
    hdrs = ["metadata_to_peer_node.h"],
    repository = "@envoy",
    visibility = ["//visibility:public"],
    deps = [
        "//extensions/common:context",
        "//extensions/common:metadata_object_lib",
        "//src/envoy/metadata_to_peer_node/config:metadata_to_peer_node_cc_proto",
        "@envoy//envoy/network:filter_interface",
        "@envoy//envoy/network:listen_socket_interface",
        "@envoy//source/common/network:address_lib",
        "@envoy//source/common/network:utility_lib",
        "@envoy//source/exe:envoy_common_lib",
        "@envoy//source/extensions/filters/common/expr:cel_state_lib",
    ],
)

envoy_cc_test(
    name = "metadata_to_peer_node_test",
    srcs = ["metadata_to_peer_node_test.cc"],
    repository = "@envoy",
    deps = [
        ":metadata_to_peer_node_lib",
        "//src/envoy/metadata_to_peer_node/config:metadata_to_peer_node_cc_proto",
        "@envoy//source/common/network:socket_option_lib",
        "@envoy//test/mocks:common_lib",
        "@envoy//test/mocks/network:network_mocks",
        "@envoy//test/mocks/stats:stats_mocks",
    ],
)

envoy_cc_extension(
    name = "config_lib",
    srcs = ["config.cc"],
    repository = "@envoy",
    deps = [
        ":metadata_to_peer_node_lib",
        "//src/envoy/metadata_to_peer_node/config:metadata_to_peer_node_cc_proto",
        "@envoy//envoy/registry",
        "@envoy//envoy/server:filter_config_interface",
        "@envoy//source/exe:envoy_common_lib",
    ],
)
