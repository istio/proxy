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
)

envoy_cc_library(
    name = "config_lib",
    srcs = ["config.cc"],
    hdrs = ["config.h"],
    repository = "@envoy",
    visibility = ["//visibility:public"],
    deps = [
        ":config_cc_proto",
        ":sni_verifier_lib",
        "@envoy//source/exe:envoy_common_lib",
    ],
)

envoy_cc_library(
    name = "sni_verifier_lib",
    srcs = ["sni_verifier.cc"],
    hdrs = ["sni_verifier.h"],
    external_deps = ["ssl"],
    repository = "@envoy",
    deps = [
        "@envoy//source/exe:envoy_common_lib",
    ],
)

envoy_cc_test(
    name = "sni_verifier_test",
    srcs = ["sni_verifier_test.cc"],
    repository = "@envoy",
    deps = [
        ":config_lib",
        ":sni_verifier_lib",
        "@envoy//test/extensions/filters/listener/tls_inspector:tls_utility_lib",
        "@envoy//test/mocks/network:network_mocks",
        "@envoy//test/mocks/server:server_mocks",
    ],
)

cc_proto_library(
    name = "config_cc_proto",
    deps = ["config_proto"],
)

proto_library(
    name = "config_proto",
    srcs = ["config.proto"],
)
