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
    "envoy_cc_test_library",
)

envoy_cc_library(
    name = "authenticator_essential_lib",
    srcs = [
        "authn_utils.cc",
        "connection_context.cc",
        "filter_context.cc",
    ],
    hdrs = [
        "authn_utils.h",
        "connection_context.h",
        "filter_context.h",
    ],
    repository = "@envoy",
    visibility = ["//visibility:public"],
    deps = [
        "//extensions/common:json_util",
        "//external:security_authentication_config_cc_proto",
        "//src/envoy/utils:filter_names_lib",
        "//src/envoy/utils:utils_lib",
        "//src/istio/authn:context_proto_cc_proto",
        "@envoy//source/common/http:headers_lib",
        "@envoy//source/exe:envoy_common_lib",
    ],
)

envoy_cc_library(
    name = "request_authenticator_lib",
    srcs = [
        "request_authenticator.cc",
    ],
    hdrs = [
        "request_authenticator.h",
    ],
    repository = "@envoy",
    visibility = ["//visibility:public"],
    deps = [
        ":authenticator_essential_lib",
    ],
)

envoy_cc_library(
    name = "peer_authenticator_lib",
    srcs = [
        "peer_authenticator.cc",
    ],
    hdrs = [
        "peer_authenticator.h",
    ],
    repository = "@envoy",
    visibility = ["//visibility:public"],
    deps = [
        ":authenticator_essential_lib",
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
    name = "request_authenticator_test",
    srcs = ["request_authenticator_test.cc"],
    repository = "@envoy",
    deps = [
        ":request_authenticator_lib",
        ":test_utils",
        "@envoy//test/mocks/http:http_mocks",
        "@envoy//test/test_common:utility_lib",
    ],
)

envoy_cc_test(
    name = "connection_context_test",
    srcs = ["connection_context_test.cc"],
    repository = "@envoy",
    deps = [
        ":request_authenticator_lib",
        ":test_utils",
        "@envoy//test/mocks/network:network_mocks",
        "@envoy//test/mocks/ssl:ssl_mocks",
        "@envoy//test/test_common:utility_lib",
    ],
)

envoy_cc_test(
    name = "peer_authenticator_test",
    srcs = ["peer_authenticator_test.cc"],
    repository = "@envoy",
    deps = [
        ":peer_authenticator_lib",
        ":test_utils",
        "@envoy//test/mocks/http:http_mocks",
        "@envoy//test/test_common:utility_lib",
    ],
)

envoy_cc_test(
    name = "filter_context_test",
    srcs = ["filter_context_test.cc"],
    repository = "@envoy",
    deps = [
        ":authenticator_essential_lib",
        ":test_utils",
        "@envoy//test/mocks/http:http_mocks",
        "@envoy//test/test_common:utility_lib",
    ],
)

envoy_cc_test(
    name = "authn_utils_test",
    srcs = ["authn_utils_test.cc"],
    repository = "@envoy",
    deps = [
        ":authenticator_essential_lib",
        ":test_utils",
        "@envoy//test/test_common:utility_lib",
    ],
)
