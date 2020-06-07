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
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
)
load(
    "@envoy//bazel/wasm:wasm.bzl",
    "wasm_cc_binary",
)

wasm_cc_binary(
    name = "authn_filter.wasm",
    srcs = [
        "base.cc",
        "base.h",
        "cert.h",
        "connection_context.cc",
        "connection_context.h",
        "filter.cc",
        "filter.h",
        "filter_context.cc",
        "filter_context.h",
        "peer.cc",
        "peer.h",
    ],
    copts = ["-UNULL_PLUGIN"],
    deps = [
        "//external:authentication_policy_config_cc_proto",
        "//src/envoy/utils:filter_names_lib",
        "//src/istio/authn:context_proto_cc_wasm",
        "@com_google_absl_wasm//absl/strings",
        "@com_google_absl_wasm//absl/types:optional",
        "@com_google_protobuf//:protobuf",
        "@proxy_wasm_cpp_sdk//:proxy_wasm_intrinsics_full",
    ],
)
