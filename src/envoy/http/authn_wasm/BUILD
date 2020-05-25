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

package(default_visibility = ["//visibility:public"])

load(
    "//:wasm.bzl",
    "wasm_cc_binary",
    "wasm_cc_library",
)

wasm_cc_library(
    name = "cert_interface",
    hdrs = ["cert.h"],
)

wasm_cc_library(
    name = "connection_context_interface",
    hdrs = ["connection_context.h"],
    deps = [
        ":cert_interface",
        "@proxy_wasm_cpp_sdk//:proxy_wasm_intrinsics",
    ],
)

wasm_cc_library(
    name = "filter_context_lib",
    hdrs = ["filter_context.h"],
    deps = [
        ":cert_interface",
        ":connection_context_interface",
        "//external:authentication_policy_config_cc_proto",
        "//src/envoy/utils:filter_names_lib",
        "//src/istio/authn:context_proto_cc_wasm",
        "@com_google_abseil//absl/strings",
        "@proxy_wasm_cpp_sdk//:proxy_wasm_intrinsics",
    ],
)

wasm_cc_library(
    name = "authn_filter_interface",
    hdrs = ["filter.h"],
    deps = [
        "//src/envoy/http/authn_wasm/authenticator:base",
        "@proxy_wasm_cpp_sdk//:proxy_wasm_intrinsics",
    ],
)

wasm_cc_binary(
    name = "authn_wasm_binary",
    srcs = ["filter.cc"],
    deps = [
        ":authn_filter_interface",
        ":connection_context_interface",
        "//external:authentication_policy_config_cc_proto",
        "//src/envoy/http/authn_wasm/authenticator:peer",
        "@com_google_protobuf//:protobuf",
    ],
)
