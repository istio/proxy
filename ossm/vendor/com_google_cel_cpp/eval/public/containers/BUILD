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
# TODO: Expose this in a public API.

package_group(
    name = "cel_internal",
    packages = ["//eval/..."],
)

cc_library(
    name = "field_access",
    srcs = [
        "field_access.cc",
    ],
    hdrs = [
        "field_access.h",
    ],
    deps = [
        "//eval/public:cel_options",
        "//eval/public:cel_value",
        "//eval/public/structs:cel_proto_wrapper",
        "//eval/public/structs:field_access_impl",
        "//internal:status_macros",
        "@com_google_absl//absl/status",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "container_backed_list_impl",
    srcs = [
    ],
    hdrs = [
        "container_backed_list_impl.h",
    ],
    deps = [
        "//eval/public:cel_value",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "container_backed_map_impl",
    srcs = [
        "container_backed_map_impl.cc",
    ],
    hdrs = [
        "container_backed_map_impl.h",
    ],
    deps = [
        "//eval/public:cel_value",
        "@com_google_absl//absl/container:node_hash_map",
        "@com_google_absl//absl/hash",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/types:optional",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "field_backed_list_impl",
    hdrs = [
        "field_backed_list_impl.h",
    ],
    deps = [
        ":internal_field_backed_list_impl",
        "//eval/public:cel_value",
        "//eval/public/structs:cel_proto_wrapper",
    ],
)

cc_library(
    name = "field_backed_map_impl",
    hdrs = [
        "field_backed_map_impl.h",
    ],
    deps = [
        ":internal_field_backed_map_impl",
        "//eval/public:cel_value",
        "//eval/public/structs:cel_proto_wrapper",
        "@com_google_absl//absl/status:statusor",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "container_backed_map_impl_test",
    size = "small",
    srcs = [
        "container_backed_map_impl_test.cc",
    ],
    deps = [
        ":container_backed_map_impl",
        "//eval/public:cel_value",
        "//internal:testing",
        "@com_google_absl//absl/status",
    ],
)

cc_test(
    name = "field_backed_list_impl_test",
    size = "small",
    srcs = [
        "field_backed_list_impl_test.cc",
    ],
    deps = [
        ":field_backed_list_impl",
        "//eval/testutil:test_message_cc_proto",
        "//internal:testing",
        "//testutil:util",
    ],
)

cc_test(
    name = "field_backed_map_impl_test",
    size = "small",
    srcs = [
        "field_backed_map_impl_test.cc",
    ],
    deps = [
        ":field_backed_map_impl",
        "//eval/testutil:test_message_cc_proto",
        "//internal:testing",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "field_access_test",
    srcs = ["field_access_test.cc"],
    deps = [
        ":field_access",
        "//eval/public:cel_value",
        "//eval/public/structs:cel_proto_wrapper",
        "//eval/public/testing:matchers",
        "//eval/testutil:test_message_cc_proto",
        "//internal:testing",
        "//internal:time",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_cel_spec//proto/test/v1/proto3:test_all_types_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "internal_field_backed_list_impl",
    srcs = [
        "internal_field_backed_list_impl.cc",
    ],
    hdrs = [
        "internal_field_backed_list_impl.h",
    ],
    deps = [
        "//eval/public:cel_value",
        "//eval/public/structs:field_access_impl",
        "//eval/public/structs:protobuf_value_factory",
    ],
)

cc_test(
    name = "internal_field_backed_list_impl_test",
    size = "small",
    srcs = [
        "internal_field_backed_list_impl_test.cc",
    ],
    deps = [
        ":internal_field_backed_list_impl",
        "//eval/public/structs:cel_proto_wrapper",
        "//eval/testutil:test_message_cc_proto",
        "//internal:testing",
        "//testutil:util",
    ],
)

cc_library(
    name = "internal_field_backed_map_impl",
    srcs = [
        "internal_field_backed_map_impl.cc",
    ],
    hdrs = [
        "internal_field_backed_map_impl.h",
    ],
    deps = [
        "//eval/public:cel_value",
        "//eval/public/structs:field_access_impl",
        "//eval/public/structs:protobuf_value_factory",
        "//extensions/protobuf/internal:map_reflection",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "internal_field_backed_map_impl_test",
    size = "small",
    srcs = [
        "internal_field_backed_map_impl_test.cc",
    ],
    visibility = ["//visibility:private"],
    deps = [
        ":internal_field_backed_map_impl",
        "//eval/public/structs:cel_proto_wrapper",
        "//eval/testutil:test_message_cc_proto",
        "//internal:testing",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
    ],
)
