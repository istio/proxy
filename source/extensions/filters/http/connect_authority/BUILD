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
    name = "connect_authority",
    srcs = ["filter.cc"],
    hdrs = ["filter.h"],
    repository = "@envoy",
    deps = [
        ":config_cc_proto",
        "//source/extensions/filters/listener/set_internal_dst_address:filter_lib",
        "@envoy//envoy/registry",
        "@envoy//source/common/http:utility_lib",
        "@envoy//source/extensions/filters/http/common:factory_base_lib",
        "@envoy//source/extensions/filters/http/common:pass_through_filter_lib",
        "@envoy//source/extensions/filters/network/common:factory_base_lib",
    ],
)

envoy_proto_library(
    name = "config",
    srcs = ["config.proto"],
)
