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
)

envoy_cc_library(
    name = "tcp_cluster_rewrite_lib",
    srcs = ["tcp_cluster_rewrite.cc"],
    hdrs = ["tcp_cluster_rewrite.h"],
    repository = "@envoy",
    deps = [
        "//external:tcp_cluster_rewrite_config_cc_proto",
        "@envoy//source/exe:envoy_common_lib",
    ],
)

envoy_cc_library(
    name = "config_lib",
    srcs = ["config.cc"],
    hdrs = ["config.h"],
    repository = "@envoy",
    deps = [
        ":tcp_cluster_rewrite_lib",
        "//external:tcp_cluster_rewrite_config_cc_proto",
        "@envoy//source/exe:envoy_common_lib",
    ],
)

envoy_cc_test(
    name = "tcp_cluster_rewrite_test",
    srcs = ["tcp_cluster_rewrite_test.cc"],
    repository = "@envoy",
    deps = [
        ":config_lib",
        ":tcp_cluster_rewrite_lib",
        "@envoy//test/mocks/network:network_mocks",
        "@envoy//test/mocks/server:server_mocks",
        "@envoy//test/mocks/stream_info:stream_info_mocks",
    ],
)

envoy_cc_test(
    name = "config_test",
    srcs = ["config_test.cc"],
    repository = "@envoy",
    deps = [
        ":config_lib",
        "@envoy//test/mocks/server:server_mocks",
    ],
)
