# Copyright 2022 Google LLC
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

package(
    # Under active development, not yet being released.
    default_visibility = ["//visibility:public"],
)

licenses(["notice"])

cc_library(
    name = "memory_manager",
    srcs = ["memory_manager.cc"],
    hdrs = ["memory_manager.h"],
    deps = [
        "//common:memory",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/base:nullability",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "memory_manager_test",
    srcs = ["memory_manager_test.cc"],
    deps = [
        ":memory_manager",
        "//common:memory",
        "//internal:testing",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "ast_converters",
    hdrs = ["ast_converters.h"],
    deps = [
        "//common:ast",
        "//common:ast_proto",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/status:statusor",
        "@com_google_cel_spec//proto/cel/expr:checked_cc_proto",
        "@com_google_cel_spec//proto/cel/expr:syntax_cc_proto",
    ],
)

cc_library(
    name = "runtime_adapter",
    srcs = ["runtime_adapter.cc"],
    hdrs = ["runtime_adapter.h"],
    deps = [
        ":ast_converters",
        "//internal:status_macros",
        "//runtime",
        "//runtime:runtime_builder",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_cel_spec//proto/cel/expr:checked_cc_proto",
        "@com_google_cel_spec//proto/cel/expr:syntax_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "enum_adapter",
    srcs = ["enum_adapter.cc"],
    hdrs = ["enum_adapter.h"],
    deps = [
        "//runtime:type_registry",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "type",
    srcs = [
        "type_introspector.cc",
    ],
    hdrs = [
        "type_introspector.h",
    ],
    deps = [
        "//common:type",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/types:optional",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "type_test",
    srcs = [
        "type_introspector_test.cc",
    ],
    deps = [
        ":type",
        "//common:type",
        "//common:type_kind",
        "//internal:testing",
        "@com_google_absl//absl/types:optional",
        "@com_google_cel_spec//proto/cel/expr/conformance/proto2:test_all_types_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "value",
    hdrs = [
        "type_reflector.h",
        "value.h",
    ],
    deps = [
        ":type",
        "//common:memory",
        "//common:type",
        "//common:value",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/meta:type_traits",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:cord",
        "@com_google_protobuf//:duration_cc_proto",
        "@com_google_protobuf//:protobuf",
        "@com_google_protobuf//:struct_cc_proto",
        "@com_google_protobuf//:timestamp_cc_proto",
        "@com_google_protobuf//:wrappers_cc_proto",
    ],
)

cc_test(
    name = "value_test",
    srcs = [
        "value_test.cc",
    ],
    deps = [
        ":value",
        "//base:attributes",
        "//common:casting",
        "//common:value",
        "//common:value_kind",
        "//common:value_testing",
        "//internal:testing",
        "@com_google_absl//absl/log:absl_check",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/time",
        "@com_google_cel_spec//proto/cel/expr/conformance/proto2:test_all_types_cc_proto",
        "@com_google_protobuf//:any_cc_proto",
        "@com_google_protobuf//:duration_cc_proto",
        "@com_google_protobuf//:protobuf",
        "@com_google_protobuf//:struct_cc_proto",
        "@com_google_protobuf//:timestamp_cc_proto",
        "@com_google_protobuf//:wrappers_cc_proto",
    ],
)

cc_test(
    name = "value_end_to_end_test",
    srcs = ["value_end_to_end_test.cc"],
    deps = [
        ":runtime_adapter",
        "//common:value",
        "//common:value_testing",
        "//internal:testing",
        "//parser",
        "//runtime",
        "//runtime:activation",
        "//runtime:runtime_options",
        "//runtime:standard_runtime_builder_factory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/time",
        "@com_google_cel_spec//proto/cel/expr/conformance/proto3:test_all_types_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "bind_proto_to_activation",
    srcs = ["bind_proto_to_activation.cc"],
    hdrs = ["bind_proto_to_activation.h"],
    deps = [
        ":value",
        "//common:casting",
        "//common:value",
        "//internal:status_macros",
        "//runtime:activation",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "bind_proto_to_activation_test",
    srcs = ["bind_proto_to_activation_test.cc"],
    deps = [
        ":bind_proto_to_activation",
        "//common:casting",
        "//common:value",
        "//common:value_testing",
        "//internal:testing",
        "//runtime:activation",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_absl//absl/types:optional",
        "@com_google_cel_spec//proto/cel/expr/conformance/proto2:test_all_types_cc_proto",
        "@com_google_protobuf//:protobuf",
        "@com_google_protobuf//:wrappers_cc_proto",
    ],
)

cc_library(
    name = "value_testing",
    testonly = True,
    hdrs = ["value_testing.h"],
    deps = [
        ":value",
        "//common:value",
        "//internal:testing",
        "@com_google_absl//absl/status",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "value_testing_test",
    srcs = ["value_testing_test.cc"],
    deps = [
        ":value",
        ":value_testing",
        "//common:value",
        "//common:value_testing",
        "//internal:proto_matchers",
        "//internal:testing",
        "@com_google_cel_spec//proto/cel/expr/conformance/proto2:test_all_types_cc_proto",
    ],
)
