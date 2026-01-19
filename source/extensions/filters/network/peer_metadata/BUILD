# Copyright 2026 Istio Authors. All Rights Reserved.
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
##########################################################################

# Ambient Peer Metadata filters
load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
)

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

envoy_cc_library(
    name = "peer_metadata",
    srcs = [
        "peer_metadata.cc",
    ],
    repository = "@envoy",
    deps = [
        "//extensions/common:metadata_object_lib",
        "//source/extensions/filters/network/peer_metadata/proto:peer_metadata_cc_proto",
        "@envoy//envoy/buffer:buffer_interface",
        "@envoy//envoy/network:address_interface",
        "@envoy//envoy/network:filter_interface",
        "@envoy//envoy/server:filter_config_interface",
        "@envoy//source/common/common:minimal_logger_lib",
        "@envoy//source/common/singleton:const_singleton",
        "@envoy//source/common/stream_info:bool_accessor_lib",
	"@envoy//source/common/tcp_proxy",
	"@envoy//source/extensions/filters/common/expr:cel_state_lib",
        "@envoy//source/extensions/filters/network/common:factory_base_lib",
    ],
)
