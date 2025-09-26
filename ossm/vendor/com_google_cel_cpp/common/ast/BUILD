load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("@rules_cc//cc:cc_test.bzl", "cc_test")

# Copyright 2025 Google LLC
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
#
# Internal AST implementation and utilities
# These are needed by various parts of the CEL-C++ library, but are not intended for public use at
# this time.
package(default_visibility = ["//visibility:public"])

cc_library(
    name = "constant_proto",
    srcs = ["constant_proto.cc"],
    hdrs = ["constant_proto.h"],
    deps = [
        "//common:constant",
        "//internal:proto_time_encoding",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/functional:overload",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:variant",
        "@com_google_cel_spec//proto/cel/expr:syntax_cc_proto",
        "@com_google_protobuf//:struct_cc_proto",
    ],
)

cc_library(
    name = "expr_proto",
    srcs = ["expr_proto.cc"],
    hdrs = ["expr_proto.h"],
    deps = [
        ":constant_proto",
        "//common:constant",
        "//common:expr",
        "//internal:status_macros",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/functional:overload",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:variant",
        "@com_google_cel_spec//proto/cel/expr:syntax_cc_proto",
        "@com_google_protobuf//:struct_cc_proto",
    ],
)

cc_test(
    name = "expr_proto_test",
    srcs = ["expr_proto_test.cc"],
    deps = [
        ":expr_proto",
        "//common:expr",
        "//internal:proto_matchers",
        "//internal:testing",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_cel_spec//proto/cel/expr:syntax_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "ast_impl",
    srcs = ["ast_impl.cc"],
    hdrs = ["ast_impl.h"],
    deps = [
        ":expr",
        "//common:ast",
        "//internal:casts",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/strings:string_view",
    ],
)

cc_test(
    name = "ast_impl_test",
    srcs = ["ast_impl_test.cc"],
    deps = [
        ":ast_impl",
        ":expr",
        "//common:ast",
        "//internal:testing",
        "@com_google_absl//absl/container:flat_hash_map",
    ],
)

cc_library(
    name = "expr",
    srcs = ["expr.cc"],
    hdrs = [
        "expr.h",
    ],
    deps = [
        "//common:constant",
        "//common:expr",
        "@com_google_absl//absl/base:no_destructor",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/functional:overload",
        "@com_google_absl//absl/types:optional",
        "@com_google_absl//absl/types:variant",
    ],
)

cc_test(
    name = "expr_test",
    srcs = [
        "expr_test.cc",
    ],
    deps = [
        ":expr",
        "//common:expr",
        "//internal:testing",
        "@com_google_absl//absl/types:variant",
    ],
)

cc_library(
    name = "source_info_proto",
    srcs = ["source_info_proto.cc"],
    hdrs = ["source_info_proto.h"],
    deps = [
        ":expr",
        ":expr_proto",
        "//internal:status_macros",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/status",
        "@com_google_cel_spec//proto/cel/expr:checked_cc_proto",
        "@com_google_cel_spec//proto/cel/expr:syntax_cc_proto",
        "@com_google_protobuf//:duration_cc_proto",
        "@com_google_protobuf//:struct_cc_proto",
        "@com_google_protobuf//:timestamp_cc_proto",
    ],
)
