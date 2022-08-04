# Copyright 2019 Istio Authors. All Rights Reserved.
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
    "envoy_package",
)

envoy_package()

envoy_cc_library(
    name = "alpn_filter",
    srcs = ["alpn_filter.cc"],
    hdrs = ["alpn_filter.h"],
    repository = "@envoy",
    deps = [
        "//external:alpn_filter_config_cc_proto",
        "@envoy//envoy/http:filter_interface",
        "@envoy//source/common/network:application_protocol_lib",
        "@envoy//source/extensions/filters/http/common:pass_through_filter_lib",
    ],
)

envoy_cc_library(
    name = "config_lib",
    srcs = ["config.cc"],
    hdrs = ["config.h"],
    repository = "@envoy",
    visibility = ["//visibility:public"],
    deps = [
        ":alpn_filter",
        "//source/extensions/common:filter_names_lib",
        "@envoy//envoy/registry",
        "@envoy//source/exe:envoy_common_lib",
        "@envoy//source/extensions/filters/http/common:factory_base_lib",
    ],
)

envoy_cc_test(
    name = "alpn_test",
    srcs = [
        "alpn_test.cc",
    ],
    repository = "@envoy",
    deps = [
        ":alpn_filter",
        ":config_lib",
        "@envoy//test/mocks/http:http_mocks",
        "@envoy//test/mocks/local_info:local_info_mocks",
        "@envoy//test/mocks/network:network_mocks",
        "@envoy//test/mocks/protobuf:protobuf_mocks",
        "@envoy//test/mocks/upstream:upstream_mocks",
    ],
)

envoy_cc_test(
    name = "config_test",
    srcs = [
        "config_test.cc",
    ],
    repository = "@envoy",
    deps = [
        ":alpn_filter",
        ":config_lib",
        "@envoy//test/mocks/server:server_mocks",
        "@envoy//test/test_common:utility_lib",
    ],
)
