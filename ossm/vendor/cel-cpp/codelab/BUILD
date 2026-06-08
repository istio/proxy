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
load("@rules_cc//cc:cc_test.bzl", "cc_test")

licenses(["notice"])

package(default_visibility = ["//visibility:public"])

exports_files(
    srcs = glob([
        "exercise*.h",
        "exercise*_test.cc",
    ]),
    visibility = ["//codelab/solutions:__pkg__"],
)

# Exclude tests from tap and glob runs since they start failing for the codelab.
# The solutions directory has test targets that are included to catch breaking changes.
EXERCISE_TEST_TAGS = [
    "manual",
    "notap",
    "norapid",
]

cc_library(
    name = "exercise1",
    srcs = ["exercise1.cc"],
    hdrs = ["exercise1.h"],
    tags = [
        "manual",
        "nobuilder",
    ],
    deps = [
        "//eval/public:activation",
        "//eval/public:builtin_func_registrar",
        "//eval/public:cel_expr_builder_factory",
        "//eval/public:cel_expression",
        "//eval/public:cel_options",
        "//eval/public:cel_value",
        "//internal:status_macros",
        "//parser",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_cel_spec//proto/cel/expr:syntax_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "exercise1_test",
    srcs = ["exercise1_test.cc"],
    tags = EXERCISE_TEST_TAGS,
    deps = [
        ":exercise1",
        "//internal:testing",
        "@com_google_absl//absl/status",
    ],
)

cc_library(
    name = "exercise2",
    srcs = ["exercise2.cc"],
    hdrs = ["exercise2.h"],
    deps = [
        ":cel_compiler",
        "//compiler",
        "//compiler:compiler_factory",
        "//compiler:standard_library",
        "//eval/public:activation",
        "//eval/public:builtin_func_registrar",
        "//eval/public:cel_expr_builder_factory",
        "//eval/public:cel_expression",
        "//eval/public:cel_options",
        "//eval/public:cel_value",
        "//internal:status_macros",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_cel_spec//proto/cel/expr:checked_cc_proto",
        "@com_google_cel_spec//proto/cel/expr:syntax_cc_proto",
        "@com_google_googleapis//google/rpc/context:attribute_context_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "exercise2_test",
    srcs = ["exercise2_test.cc"],
    tags = EXERCISE_TEST_TAGS,
    deps = [
        ":exercise2",
        "//internal:testing",
        "@com_google_absl//absl/status",
        "@com_google_googleapis//google/rpc/context:attribute_context_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "exercise3_test",
    srcs = ["exercise3_test.cc"],
    tags = EXERCISE_TEST_TAGS,
    deps = [
        ":exercise2",
        "//internal:testing",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_googleapis//google/rpc/context:attribute_context_cc_proto",
    ],
)

cc_library(
    name = "cel_compiler",
    hdrs = ["cel_compiler.h"],
    deps = [
        "//checker:validation_result",
        "//common:ast_proto",
        "//compiler",
        "//internal:status_macros",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_cel_spec//proto/cel/expr:checked_cc_proto",
    ],
)

cc_test(
    name = "cel_compiler_test",
    srcs = ["cel_compiler_test.cc"],
    deps = [
        ":cel_compiler",
        "//common:decl",
        "//common:type",
        "//compiler",
        "//compiler:compiler_factory",
        "//compiler:standard_library",
        "//eval/public:activation",
        "//eval/public:activation_bind_helper",
        "//eval/public:builtin_func_registrar",
        "//eval/public:cel_expr_builder_factory",
        "//eval/public:cel_function_adapter",
        "//eval/public:cel_value",
        "//eval/public/testing:matchers",
        "//internal:testing",
        "@com_google_absl//absl/log:absl_check",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_googleapis//google/rpc/context:attribute_context_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "exercise4",
    srcs = ["exercise4.cc"],
    hdrs = ["exercise4.h"],
    deps = [
        ":cel_compiler",
        "//compiler",
        "//compiler:compiler_factory",
        "//compiler:standard_library",
        "//eval/public:activation",
        "//eval/public:activation_bind_helper",
        "//eval/public:builtin_func_registrar",
        "//eval/public:cel_expr_builder_factory",
        "//eval/public:cel_expression",
        "//eval/public:cel_options",
        "//eval/public:cel_value",
        "//internal:status_macros",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_cel_spec//proto/cel/expr:checked_cc_proto",
        "@com_google_googleapis//google/rpc/context:attribute_context_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "exercise4_test",
    srcs = ["exercise4_test.cc"],
    tags = EXERCISE_TEST_TAGS,
    deps = [
        ":exercise4",
        "//internal:testing",
        "@com_google_googleapis//google/rpc/context:attribute_context_cc_proto",
        "@com_google_protobuf//:protobuf",
        "@com_google_protobuf//:struct_cc_proto",
    ],
)
