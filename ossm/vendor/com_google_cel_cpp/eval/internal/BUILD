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

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

cc_library(
    name = "interop",
    hdrs = ["interop.h"],
    deps = ["//common:legacy_value"],
)

cc_library(
    name = "cel_value_equal",
    srcs = ["cel_value_equal.cc"],
    hdrs = ["cel_value_equal.h"],
    deps = [
        "//base:kind",
        "//eval/public:cel_number",
        "//eval/public:cel_value",
        "//eval/public:message_wrapper",
        "//eval/public/structs:legacy_type_adapter",
        "//eval/public/structs:legacy_type_info_apis",
        "//internal:number",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:optional",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "cel_value_equal_test",
    srcs = ["cel_value_equal_test.cc"],
    deps = [
        ":cel_value_equal",
        "//eval/public:cel_value",
        "//eval/public:message_wrapper",
        "//eval/public/containers:container_backed_list_impl",
        "//eval/public/containers:container_backed_map_impl",
        "//eval/public/structs:cel_proto_wrapper",
        "//eval/public/structs:trivial_legacy_type_info",
        "//eval/testutil:test_message_cc_proto",
        "//internal:testing",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:span",
        "@com_google_absl//absl/types:variant",
        "@com_google_googleapis//google/rpc/context:attribute_context_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "errors",
    srcs = ["errors.cc"],
    hdrs = ["errors.h"],
    deps = [
        "//runtime/internal:errors",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "adapter_activation_impl",
    srcs = ["adapter_activation_impl.cc"],
    hdrs = ["adapter_activation_impl.h"],
    deps = [
        ":interop",
        "//base:attributes",
        "//common:memory",
        "//common:value",
        "//eval/public:base_activation",
        "//eval/public:cel_value",
        "//extensions/protobuf:memory_manager",
        "//internal:status_macros",
        "//runtime:activation_interface",
        "//runtime:function_overload_reference",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:optional",
        "@com_google_absl//absl/types:span",
        "@com_google_protobuf//:protobuf",
    ],
)
