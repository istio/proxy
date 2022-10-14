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

licenses(["notice"])  # Apache 2

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_cc_test",
    "envoy_package",
    "envoy_proto_library",
)
load(
    "@envoy//test/extensions:extensions_build_system.bzl",
    "envoy_extension_cc_test",
)

envoy_package()

envoy_cc_library(
    name = "stats_plugin",
    srcs = [
        "plugin.cc",
    ],
    hdrs = [
        "plugin.h",
    ],
    repository = "@envoy",
    visibility = ["//visibility:public"],
    deps = [
        "//extensions/common:context",
        "//extensions/common:json_util",
        "@proxy_wasm_cpp_host//:null_lib",
        "@proxy_wasm_cpp_sdk//contrib:contrib_lib",
    ],
)

envoy_proto_library(
    name = "config",
    srcs = ["config.proto"],
)

envoy_extension_cc_test(
    name = "plugin_test",
    size = "small",
    srcs = ["plugin_test.cc"],
    extension_names = ["envoy.filters.http.wasm"],
    repository = "@envoy",
    deps = [
        ":stats_plugin",
        "//external:abseil_hash_testing",
        "@envoy//test/test_common:wasm_lib",
    ],
)
