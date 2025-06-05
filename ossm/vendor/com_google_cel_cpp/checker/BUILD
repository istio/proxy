# Copyright 2024 Google LLC
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
package(
    # Under active development, not yet being released.
    default_visibility = ["//visibility:public"],
)

cc_library(
    name = "checker_options",
    hdrs = ["checker_options.h"],
)

cc_library(
    name = "type_check_issue",
    srcs = ["type_check_issue.cc"],
    hdrs = ["type_check_issue.h"],
    deps = [
        "//common:source",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/strings:string_view",
    ],
)

cc_test(
    name = "type_check_issue_test",
    srcs = ["type_check_issue_test.cc"],
    deps = [
        ":type_check_issue",
        "//common:source",
        "//internal:testing",
    ],
)

cc_library(
    name = "validation_result",
    hdrs = ["validation_result.h"],
    deps = [
        ":type_check_issue",
        "//common:ast",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/types:span",
    ],
)

cc_test(
    name = "validation_result_test",
    srcs = ["validation_result_test.cc"],
    deps = [
        ":type_check_issue",
        ":validation_result",
        "//base/ast_internal:ast_impl",
        "//internal:testing",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:status_matchers",
    ],
)

cc_library(
    name = "type_checker",
    hdrs = ["type_checker.h"],
    deps = [
        ":validation_result",
        "//common:ast",
        "@com_google_absl//absl/status:statusor",
    ],
)

cc_library(
    name = "type_checker_builder",
    srcs = ["type_checker_builder.cc"],
    hdrs = ["type_checker_builder.h"],
    deps = [
        ":checker_options",
        ":type_checker",
        "//checker/internal:type_check_env",
        "//checker/internal:type_checker_impl",
        "//common:decl",
        "//common:type",
        "//internal:status_macros",
        "//internal:well_known_types",
        "//parser:macro",
        "@com_google_absl//absl/base:no_destructor",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/log:absl_check",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "type_checker_builder_test",
    srcs = ["type_checker_builder_test.cc"],
    deps = [
        ":type_checker_builder",
        ":validation_result",
        "//checker/internal:test_ast_helpers",
        "//common:decl",
        "//common:type",
        "//internal:testing",
        "//internal:testing_descriptor_pool",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:status_matchers",
    ],
)

cc_library(
    name = "standard_library",
    srcs = ["standard_library.cc"],
    hdrs = ["standard_library.h"],
    deps = [
        ":type_checker_builder",
        "//base:builtins",
        "//checker/internal:builtins_arena",
        "//common:constant",
        "//common:decl",
        "//common:type",
        "//internal:status_macros",
        "@com_google_absl//absl/base:no_destructor",
        "@com_google_absl//absl/status",
    ],
)

cc_test(
    name = "standard_library_test",
    srcs = ["standard_library_test.cc"],
    deps = [
        ":standard_library",
        ":type_checker",
        ":type_checker_builder",
        ":validation_result",
        "//base/ast_internal:ast_impl",
        "//base/ast_internal:expr",
        "//checker/internal:test_ast_helpers",
        "//common:ast",
        "//common:constant",
        "//common:decl",
        "//common:type",
        "//internal:testing",
        "//internal:testing_descriptor_pool",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "optional",
    srcs = ["optional.cc"],
    hdrs = ["optional.h"],
    deps = [
        ":type_checker_builder",
        "//base:builtins",
        "//checker/internal:builtins_arena",
        "//common:decl",
        "//common:type",
        "//internal:status_macros",
        "@com_google_absl//absl/base:no_destructor",
        "@com_google_absl//absl/status",
    ],
)

cc_test(
    name = "optional_test",
    srcs = ["optional_test.cc"],
    deps = [
        ":checker_options",
        ":optional",
        ":standard_library",
        ":type_check_issue",
        ":type_checker",
        ":type_checker_builder",
        "//base/ast_internal:ast_impl",
        "//base/ast_internal:expr",
        "//checker/internal:test_ast_helpers",
        "//internal:testing",
        "//internal:testing_descriptor_pool",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_absl//absl/strings",
    ],
)
