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
    "envoy_cc_test",
    "envoy_extension_package",
    "envoy_proto_library",
)

envoy_extension_package()

envoy_cc_extension(
    name = "filter_lib",
    srcs = ["filter.cc"],
    hdrs = ["filter.h"],
    repository = "@envoy",
    deps = [
        ":config_cc_proto",
        "//extensions/common:context",
        "//extensions/common:metadata_object_lib",
        "//extensions/common:proto_util",
        "//source/extensions/common/workload_discovery:api_lib",
        "@envoy//envoy/registry",
        "@envoy//source/common/common:base64_lib",
        "@envoy//source/common/common:hash_lib",
        "@envoy//source/common/http:header_utility_lib",
        "@envoy//source/common/http:utility_lib",
        "@envoy//source/common/network:utility_lib",
        "@envoy//source/extensions/filters/common/expr:cel_state_lib",
        "@envoy//source/extensions/filters/http/common:factory_base_lib",
        "@envoy//source/extensions/filters/http/common:pass_through_filter_lib",
    ],
)

envoy_proto_library(
    name = "config",
    srcs = ["config.proto"],
)

envoy_cc_test(
    name = "filter_test",
    srcs = ["filter_test.cc"],
    repository = "@envoy",
    deps = [
        ":filter_lib",
        "@envoy//source/common/network:address_lib",
        "@envoy//test/common/stream_info:test_util",
        "@envoy//test/mocks/server:factory_context_mocks",
        "@envoy//test/mocks/stream_info:stream_info_mocks",
        "@envoy//test/test_common:logging_lib",
    ],
)
