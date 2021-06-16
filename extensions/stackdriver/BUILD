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

package(default_visibility = ["//visibility:public"])

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
)

envoy_cc_library(
    name = "stackdriver_plugin",
    srcs = [
        "stackdriver.cc",
        "stackdriver_plugin_factory.cc",
    ],
    hdrs = [
        "stackdriver.h",
    ],
    repository = "@envoy",
    deps = [
        "//extensions/common:context",
        "//extensions/common:proto_util",
        "//extensions/stackdriver/common:constants",
        "//extensions/stackdriver/config/v1alpha1:stackdriver_plugin_config_cc_proto",
        "//extensions/stackdriver/log:exporter",
        "//extensions/stackdriver/log:logger",
        "//extensions/stackdriver/metric",
        "@io_opencensus_cpp//opencensus/exporters/stats/stackdriver:stackdriver_exporter",
        "@proxy_wasm_cpp_host//:null_lib",
        "@proxy_wasm_cpp_sdk//contrib:contrib_lib",
    ],
)
