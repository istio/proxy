# Copyright 2020 Istio Authors. All Rights Reserved.
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

licenses(["notice"])  # Apache 2

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_cc_test",
    "envoy_package",
)
load(
    "@envoy//test/extensions:extensions_build_system.bzl",
    "envoy_extension_cc_test",
)

envoy_package()

envoy_cc_library(
    name = "attributegen_plugin",
    srcs = [
        "plugin.cc",
    ],
    hdrs = [
        "plugin.h",
    ],
    repository = "@envoy",
    visibility = ["//visibility:public"],
    deps = [
        ":config_cc_proto",
        "@proxy_wasm_cpp_host//:null_lib",
        "@proxy_wasm_cpp_sdk//contrib:contrib_lib",
    ],
)

cc_proto_library(
    name = "config_cc_proto",
    visibility = ["//visibility:public"],
    deps = ["config_proto"],
)

proto_library(
    name = "config_proto",
    srcs = ["config.proto"],
)

envoy_extension_cc_test(
    name = "plugin_test",
    size = "small",
    srcs = ["plugin_test.cc"],
    data = [
        "//extensions/attributegen/testdata",
    ],
    extension_names = ["envoy.filters.http.wasm"],
    repository = "@envoy",
    deps = [
        ":attributegen_plugin",
        "@envoy//source/extensions/filters/http/wasm:wasm_filter_lib",
        "@envoy//test/extensions/common/wasm:wasm_runtime",
        "@envoy//test/mocks/grpc:grpc_mocks",
        "@envoy//test/mocks/http:http_mocks",
        "@envoy//test/mocks/network:network_mocks",
        "@envoy//test/mocks/server:server_mocks",
        "@envoy//test/mocks/ssl:ssl_mocks",
        "@envoy//test/mocks/stream_info:stream_info_mocks",
        "@envoy//test/mocks/thread_local:thread_local_mocks",
        "@envoy//test/mocks/upstream:upstream_mocks",
        "@envoy//test/test_common:environment_lib",
        "@envoy//test/test_common:utility_lib",
        "@envoy//test/test_common:wasm_lib",
    ],
)
