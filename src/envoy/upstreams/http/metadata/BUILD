# Copyright 2021 Istio Authors. All Rights Reserved.
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
    name = "config",
    srcs = ["config.cc"],
    hdrs = ["config.h"],
    repository = "@envoy",
    visibility = ["//visibility:public"],
    deps = [
        ":upstream_request_lib",
        "@com_google_absl//absl/types:optional",
        "@envoy//include/envoy/registry",
        "@envoy//include/envoy/router:router_interface",
        "@envoy//include/envoy/upstream:load_balancer_interface",
        "@envoy//include/envoy/upstream:thread_local_cluster_interface",
        "@envoy//source/common/protobuf",
    ],
)

envoy_cc_library(
    name = "upstream_request_lib",
    srcs = ["upstream_request.cc"],
    hdrs = ["upstream_request.h"],
    repository = "@envoy",
    deps = [
        "@com_google_absl//absl/types:optional",
        "@envoy//include/envoy/http:codec_interface",
        "@envoy//include/envoy/router:router_interface",
        "@envoy//include/envoy/stream_info:stream_info_interface",
        "@envoy//include/envoy/upstream:host_description_interface",
        "@envoy//include/envoy/upstream:load_balancer_interface",
        "@envoy//include/envoy/upstream:thread_local_cluster_interface",
        "@envoy//source/extensions/upstreams/http/http:upstream_request_lib",
    ],
)

envoy_cc_test(
    name = "integration_test",
    srcs = ["integration_test.cc"],
    repository = "@envoy",
    deps = [
        ":config",
        "@envoy//include/envoy/network:address_interface",
        "@envoy//source/common/http:codec_client_lib",
        "@envoy//test/integration:http_integration_lib",
        "@envoy//test/test_common:environment_lib",
        "@envoy//test/test_common:registry_lib",
        "@envoy_api//envoy/config/bootstrap/v3:pkg_cc_proto",
    ],
)

envoy_cc_test(
    name = "upstream_request_test",
    srcs = ["upstream_request_test.cc"],
    repository = "@envoy",
    deps = [
        ":upstream_request_lib",
        "@envoy//test/common/http:common_lib",
        "@envoy//test/mocks/http:stream_encoder_mock",
        "@envoy//test/mocks/router:router_mocks",
        "@envoy//test/test_common:utility_lib",
    ],
)
