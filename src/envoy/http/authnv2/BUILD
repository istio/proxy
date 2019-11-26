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
    "envoy_cc_library",
    "envoy_cc_test",
    "envoy_cc_test_library",
)

package(default_visibility = ["//visibility:public"])

envoy_cc_library(
    name = "filter_lib",
    srcs = [
        "filter.cc",
        "filter_factory.cc",
    ],
    hdrs = [
        "filter.h",
    ],
    repository = "@envoy",
    deps = [
        "//external:authentication_policy_config_cc_proto",
        "//include/istio/utils:attribute_names_header",
        "//src/envoy/http/jwt_auth:jwt_lib",
        "//src/envoy/utils:authn_lib",
        "//src/envoy/utils:filter_names_lib",
        "//src/envoy/utils:utils_lib",
        "//src/istio/authn:context_proto_cc_proto",
        "@envoy//source/common/http:headers_lib",
        "@envoy//source/exe:envoy_common_lib",
    ],
)

envoy_cc_test_library(
    name = "test_utils",
    hdrs = ["test_utils.h"],
    repository = "@envoy",
    deps = [
        "//src/istio/authn:context_proto_cc_proto",
    ],
)

envoy_cc_test(
    name = "filter_test",
    srcs = ["filter_test.cc"],
    repository = "@envoy",
    deps = [
        ":filter_lib",
        ":test_utils",
        "@envoy//source/common/http:header_map_lib",
        "@envoy//test/mocks/http:http_mocks",
        "@envoy//test/mocks/network:network_mocks",
        "@envoy//test/mocks/ssl:ssl_mocks",
        "@envoy//test/test_common:simulated_time_system_lib",
        "@envoy//test/test_common:utility_lib",
    ],
)
