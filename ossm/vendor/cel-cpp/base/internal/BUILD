# Copyright 2021 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

load("@rules_cc//cc:cc_library.bzl", "cc_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

cc_library(
    name = "memory_manager_testing",
    testonly = True,
    srcs = ["memory_manager_testing.cc"],
    hdrs = ["memory_manager_testing.h"],
    deps = [
        "//internal:testing",
    ],
)

cc_library(
    name = "message_wrapper",
    hdrs = ["message_wrapper.h"],
)

cc_library(
    name = "operators",
    hdrs = ["operators.h"],
    deps = [
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "unknown_set",
    srcs = ["unknown_set.cc"],
    hdrs = ["unknown_set.h"],
    deps = [
        "//base:attributes",
        "//base:function_result_set",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/base:no_destructor",
    ],
)
