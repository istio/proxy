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

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "arena_string",
    hdrs = ["arena_string.h"],
    deps = ["@com_google_absl//absl/strings:string_view"],
)

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
    name = "data_interface",
    hdrs = ["data_interface.h"],
    deps = [
        "//common:native_type",
        "@com_google_absl//absl/base:core_headers",
    ],
)

cc_test(
    name = "data_interface_test",
    srcs = ["data_interface_test.cc"],
    deps = [
        ":data_interface",
        "//common:native_type",
        "//internal:testing",
    ],
)

cc_library(
    name = "reference_count",
    srcs = ["reference_count.cc"],
    hdrs = ["reference_count.h"],
    deps = [
        "//common:arena",
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
    ],
)

cc_library(
    name = "shared_byte_string",
    srcs = ["shared_byte_string.cc"],
    hdrs = ["shared_byte_string.h"],
    deps = [
        ":arena_string",
        ":reference_count",
        "//common:allocator",
        "//common:memory",
        "@com_google_absl//absl/base",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/functional:overload",
        "@com_google_absl//absl/log:absl_check",
        "@com_google_absl//absl/meta:type_traits",
        "@com_google_absl//absl/strings:cord",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "shared_byte_string_test",
    srcs = ["shared_byte_string_test.cc"],
    deps = [
        ":reference_count",
        ":shared_byte_string",
        "//internal:testing",
        "@com_google_absl//absl/hash",
        "@com_google_absl//absl/strings:cord",
        "@com_google_absl//absl/strings:string_view",
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
