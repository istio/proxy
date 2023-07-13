# Copyright 2018 Istio Authors. All Rights Reserved.
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
    "envoy_extension_package",
    "envoy_proto_library",
)

envoy_extension_package()

envoy_cc_extension(
    name = "istio_stats",
    srcs = ["istio_stats.cc"],
    hdrs = ["istio_stats.h"],
    repository = "@envoy",
    deps = [
        ":config_cc_proto",
        "//extensions/common:metadata_object_lib",
        "//source/extensions/common:utils_lib",
        "//source/extensions/filters/network/istio_authn:config_lib",
        "@com_google_cel_cpp//eval/public:builtin_func_registrar",
        "@com_google_cel_cpp//eval/public:cel_expr_builder_factory",
        "@com_google_cel_cpp//parser",
        "@envoy//envoy/registry",
        "@envoy//envoy/server:filter_config_interface",
        "@envoy//envoy/stream_info:filter_state_interface",
        "@envoy//source/common/grpc:common_lib",
        "@envoy//source/common/http:header_map_lib",
        "@envoy//source/common/network:utility_lib",
        "@envoy//source/common/stream_info:utility_lib",
        "@envoy//source/extensions/filters/common/expr:cel_state_lib",
        "@envoy//source/extensions/filters/common/expr:context_lib",
        "@envoy//source/extensions/filters/common/expr:evaluator_lib",
        "@envoy//source/extensions/filters/http/common:factory_base_lib",
        "@envoy//source/extensions/filters/http/common:pass_through_filter_lib",
        "@envoy//source/extensions/filters/http/grpc_stats:config",
    ],
)

envoy_proto_library(
    name = "config",
    srcs = ["config.proto"],
)
