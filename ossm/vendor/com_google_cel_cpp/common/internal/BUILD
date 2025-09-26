# Copyright 2023 Google LLC
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
load("@rules_cc//cc:cc_test.bzl", "cc_test")

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "casting",
    hdrs = ["casting.h"],
    deps = [
        "//common:native_type",
        "//internal:casts",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/log:absl_check",
        "@com_google_absl//absl/meta:type_traits",
        "@com_google_absl//absl/types:optional",
    ],
)

cc_library(
    name = "reference_count",
    srcs = ["reference_count.cc"],
    hdrs = ["reference_count.h"],
    deps = [
        "//common:data",
        "//internal:new",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/log:absl_check",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "reference_count_test",
    srcs = ["reference_count_test.cc"],
    deps = [
        ":reference_count",
        "//common:data",
        "//internal:testing",
        "@com_google_absl//absl/base:nullability",
        "@com_google_protobuf//:protobuf",
        "@com_google_protobuf//:struct_cc_proto",
    ],
)

cc_library(
    name = "metadata",
    hdrs = ["metadata.h"],
    deps = ["@com_google_protobuf//:protobuf"],
)

cc_library(
    name = "byte_string",
    srcs = ["byte_string.cc"],
    hdrs = ["byte_string.h"],
    deps = [
        ":metadata",
        ":reference_count",
        "//common:allocator",
        "//common:arena",
        "//common:memory",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/functional:overload",
        "@com_google_absl//absl/hash",
        "@com_google_absl//absl/log:absl_check",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:cord",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/types:optional",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "byte_string_test",
    srcs = ["byte_string_test.cc"],
    deps = [
        ":byte_string",
        ":reference_count",
        "//common:allocator",
        "//common:memory",
        "//internal:testing",
        "@com_google_absl//absl/base:no_destructor",
        "@com_google_absl//absl/hash",
        "@com_google_absl//absl/strings:cord",
        "@com_google_absl//absl/strings:cord_test_helpers",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/types:optional",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "value_conversion",
    srcs = ["value_conversion.cc"],
    hdrs = ["value_conversion.h"],
    deps = [
        "//common:any",
        "//common:value",
        "//common:value_kind",
        "//extensions/protobuf:value",
        "//internal:proto_time_encoding",
        "//internal:status_macros",
        "//internal:time",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:cord",
        "@com_google_absl//absl/time",
        "@com_google_cel_spec//proto/cel/expr:checked_cc_proto",
        "@com_google_cel_spec//proto/cel/expr:syntax_cc_proto",
        "@com_google_cel_spec//proto/cel/expr:value_cc_proto",
        "@com_google_googleapis//google/api/expr/v1alpha1:checked_cc_proto",
        "@com_google_googleapis//google/api/expr/v1alpha1:value_cc_proto",
        "@com_google_protobuf//:any_cc_proto",
        "@com_google_protobuf//:protobuf",
        "@com_google_protobuf//:struct_cc_proto",
        "@com_google_protobuf//:timestamp_cc_proto",
        "@com_google_protobuf//src/google/protobuf/io",
    ],
)
