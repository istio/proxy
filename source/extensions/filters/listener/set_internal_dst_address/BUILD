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
    "envoy_proto_library",
)

licenses(["notice"])  # Apache 2

envoy_extension_package()

envoy_cc_library(
    name = "filter_lib",
    srcs = ["filter.cc"],
    hdrs = ["filter.h"],
    repository = "@envoy",
    visibility = ["//visibility:public"],
    deps = [
        ":config_cc_proto",
        "@envoy//envoy/common:hashable_interface",
        "@envoy//envoy/network:filter_interface",
        "@envoy//envoy/registry",
        "@envoy//envoy/server:filter_config_interface",
        "@envoy//source/common/common:hash_lib",
        "@envoy//source/common/network:filter_state_dst_address_lib",
        "@envoy//source/common/network:utility_lib",
    ],
)

envoy_proto_library(
    name = "config",
    srcs = ["config.proto"],
)
