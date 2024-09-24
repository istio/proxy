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
    name = "metadata_object_lib",
    srcs = ["metadata_object.cc"],
    hdrs = ["metadata_object.h"],
    repository = "@envoy",
    deps = [
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:optional",
        "@envoy//envoy/common:hashable_interface",
        "@envoy//envoy/registry",
        "@envoy//envoy/stream_info:filter_state_interface",
        "@envoy//source/common/common:hash_lib",
    ],
)

envoy_cc_test(
    name = "metadata_object_test",
    srcs = ["metadata_object_test.cc"],
    repository = "@envoy",
    deps = [
        ":metadata_object_lib",
        "@envoy//envoy/registry",
    ],
)
