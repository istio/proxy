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
    "wasm_cc_library",
)

wasm_cc_library(
    name = "base",
    srcs = ["base.cc"],
    hdrs = ["base.h"],
    deps = [
        "//external:authentication_policy_config_cc_proto",
        "//src/envoy/http/authn_wasm:filter_context_lib",
        "//src/istio/authn:context_proto_cc_wasm",
        "@com_google_abseil//absl/strings",
    ],
)

wasm_cc_library(
    name = "peer",
    srcs = ["peer.cc"],
    hdrs = ["peer.h"],
    deps = [
        ":base",
        "//external:authentication_policy_config_cc_proto",
    ],
)
