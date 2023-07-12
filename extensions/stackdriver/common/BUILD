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

licenses(["notice"])

package(default_visibility = ["//extensions/stackdriver:__subpackages__"])

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_cc_test",
)
load(
    "@envoy//test/extensions:extensions_build_system.bzl",
    "envoy_extension_cc_test",
)

envoy_cc_library(
    name = "constants",
    hdrs = [
        "constants.h",
    ],
    repository = "@envoy",
)

envoy_cc_library(
    name = "utils",
    srcs = [
        "utils.cc",
    ],
    hdrs = [
        "utils.h",
    ],
    external_deps = ["grpc"],
    repository = "@envoy",
    deps = [
        ":constants",
        "//extensions/common:context",
        "@com_google_googleapis//google/monitoring/v3:monitoring_cc_proto",
        "@envoy_api//envoy/config/core/v3:pkg_cc_proto",
    ],
)

envoy_extension_cc_test(
    name = "utils_test",
    size = "small",
    srcs = ["utils_test.cc"],
    extension_names = ["envoy.filters.http.wasm"],
    repository = "@envoy",
    deps = [
        ":utils",
        "@envoy//test/test_common:status_utility_lib",
        "@envoy//test/test_common:wasm_lib",
    ],
)

envoy_cc_library(
    name = "metrics",
    srcs = [
        "metrics.cc",
    ],
    hdrs = [
        "metrics.h",
    ],
    repository = "@envoy",
    deps = [
        "@proxy_wasm_cpp_host//:null_lib",
    ],
)
