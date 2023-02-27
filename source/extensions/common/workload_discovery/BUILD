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
#

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_extension",
    "envoy_cc_test",
    "envoy_extension_package",
    "envoy_proto_library",
)

envoy_extension_package()

envoy_cc_extension(
    name = "api_lib",
    srcs = ["api.cc"],
    hdrs = ["api.h"],
    repository = "@envoy",
    deps = [
        ":discovery_cc_proto",
        "//extensions/common:metadata_object_lib",
        "@envoy//envoy/registry",
        "@envoy//envoy/server:bootstrap_extension_config_interface",
        "@envoy//envoy/server:factory_context_interface",
        "@envoy//envoy/singleton:manager_interface",
        "@envoy//envoy/stats:stats_macros",
        "@envoy//envoy/thread_local:thread_local_interface",
        "@envoy//source/common/common:non_copyable",
        "@envoy//source/common/config:subscription_base_interface",
        "@envoy//source/common/grpc:common_lib",
        "@envoy//source/common/init:target_lib",
    ],
)

envoy_proto_library(
    name = "discovery",
    srcs = [
        "discovery.proto",
        "extension.proto",
    ],
    deps = [
        "@envoy_api//envoy/config/core/v3:pkg",
    ],
)
