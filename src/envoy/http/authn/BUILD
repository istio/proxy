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

package(default_visibility = ["//visibility:public"])

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_cc_test",
    "envoy_proto_library",
)

envoy_proto_library(
    name = "context_proto",
    srcs = ["context.proto"],
)

envoy_cc_library(
    name = "filter_lib",
    srcs = [
        "http_filter_factory.cc",
        "http_filter.cc",
        "mtls_authentication.cc",
    ],
    hdrs = [
        "http_filter.h",
        "mtls_authentication.h"
    ],
    repository = "@envoy",
    deps = [
        ":context_proto",
        "//external:authentication_policy_config_cc_proto",
        "//src/envoy/utils:utils_lib",
        "@envoy//source/exe:envoy_common_lib",
    ],
)

cc_library(
    name = "filter_mock",
    hdrs = ["http_filter_mock.h"],
    deps = [
        ":context_proto",
        ":filter_lib",
    ],
)

envoy_cc_test(
    name = "http_filter_test",
    srcs = ["http_filter_test.cc"],
    repository = "@envoy",
    deps = [
        ":filter_mock",
        "@envoy//source/common/http:header_map_lib",
        "@envoy//test/mocks/http:http_mocks",
        "@envoy//test/test_common:utility_lib",
    ],
)

envoy_cc_test(
    name = "http_filter_integration_test",
    srcs = ["http_filter_integration_test.cc"],
    repository = "@envoy",
    data = glob(["testdata/*"]),
    #     ":testdata/*",
    #     ":testdata/envoy_source_mtls.conf",
    #     ":testdata/envoy_source_jwt.conf",
    # ],
    deps = [
        ":filter_lib",
        "@envoy//test/integration:http_integration_lib",
    ],
)
