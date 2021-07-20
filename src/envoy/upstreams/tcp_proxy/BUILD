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
    name = "upstream_lib",
    srcs = ["upstream.cc"],
    hdrs = ["upstream.h"],
    repository = "@envoy",
    deps = [
        "@envoy//include/envoy/tcp:conn_pool_interface",
        "@envoy//source/common/tcp_proxy:upstream_lib",
    ],
)

envoy_cc_test(
    name = "upstream_test",
    srcs = ["upstream_test.cc"],
    repository = "@envoy",
    deps = [
        ":upstream_lib",
        "@envoy//source/common/http:header_map_lib",
        "@envoy//source/common/http:headers_lib",
        "@envoy//test/mocks/http:http_mocks",
        "@envoy//test/mocks/tcp:tcp_mocks",
        "@envoy_api//envoy/extensions/filters/network/tcp_proxy/v3:pkg_cc_proto",
    ],
)
