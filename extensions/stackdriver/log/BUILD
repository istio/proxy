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

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_cc_test",
)

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

envoy_cc_library(
    name = "logger",
    srcs = [
        "logger.cc",
    ],
    hdrs = [
        "logger.h",
    ],
    repository = "@envoy",
    deps = [
        ":exporter",
        "//extensions/common:context",
        "//extensions/common:util",
        "//extensions/stackdriver/common:constants",
        "//extensions/stackdriver/common:utils",
        "@com_google_absl//absl/strings",
        "@com_google_googleapis//google/logging/v2:logging_cc_proto",
        "@com_googlesource_code_re2//:re2",
        "@proxy_wasm_cpp_host//:null_lib",
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
    deps = [
        "//extensions/stackdriver/common:metrics",
        "//extensions/stackdriver/common:utils",
        "@com_google_googleapis//google/logging/v2:logging_cc_proto",
        "@proxy_wasm_cpp_host//:null_lib",
    ],
)

envoy_cc_test(
    name = "logger_test",
    size = "small",
    srcs = ["logger_test.cc"],
    repository = "@envoy",
    deps = [
        ":logger",
        "//extensions/stackdriver/common:constants",
        "//extensions/stackdriver/common:utils",
        "@com_google_googleapis//google/logging/v2:logging_cc_proto",
        "@com_google_protobuf//:protobuf",
        "@envoy//test/test_common:status_utility_lib",
        "@envoy//test/test_common:wasm_lib",
    ],
)
