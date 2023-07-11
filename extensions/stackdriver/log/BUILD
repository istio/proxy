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
)
load(
    "@envoy//test/extensions:extensions_build_system.bzl",
    "envoy_extension_cc_test",
)

envoy_cc_library(
    name = "logger",
    srcs = [
        "logger.cc",
    ],
    hdrs = [
        "logger.h",
    ],
    repository = "@envoy",
    visibility = [
        "//extensions/stackdriver:__pkg__",
    ],
    deps = [
        ":exporter",
        "//extensions/common:context",
        "//extensions/stackdriver/common:constants",
        "//extensions/stackdriver/common:utils",
    ],
)

envoy_cc_library(
    name = "exporter",
    srcs = [
        "exporter.cc",
    ],
    hdrs = [
        "exporter.h",
    ],
    copts = ["-DPROXY_WASM_PROTOBUF=1"],
    repository = "@envoy",
    visibility = [
        "//extensions/stackdriver:__pkg__",
    ],
    deps = [
        "//extensions/stackdriver/common:metrics",
        "//extensions/stackdriver/common:utils",
        "@com_google_googleapis//google/logging/v2:logging_cc_proto",
        "@proxy_wasm_cpp_host//:null_lib",
    ],
)

envoy_extension_cc_test(
    name = "logger_test",
    size = "small",
    srcs = ["logger_test.cc"],
    extension_names = ["envoy.filters.http.wasm"],
    repository = "@envoy",
    deps = [
        ":logger",
        "@envoy//test/test_common:status_utility_lib",
        "@envoy//test/test_common:wasm_lib",
    ],
)
