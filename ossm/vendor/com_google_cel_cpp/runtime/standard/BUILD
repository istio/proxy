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

# Provides registrars for CEL standard definitions.
# TODO(uncreated-issue/41): CEL users shouldn't need to use these directly, instead they should prefer to
# use RegisterBuiltins when available.
package(
    # Under active development, not yet being released.
    default_visibility = ["//visibility:public"],
)

cc_library(
    name = "comparison_functions",
    srcs = [
        "comparison_functions.cc",
    ],
    hdrs = [
        "comparison_functions.h",
    ],
    deps = [
        "//base:builtins",
        "//base:function_adapter",
        "//common:value",
        "//internal:number",
        "//internal:status_macros",
        "//runtime:function_registry",
        "//runtime:runtime_options",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/time",
    ],
)

cc_test(
    name = "comparison_functions_test",
    size = "small",
    srcs = [
        "comparison_functions_test.cc",
    ],
    deps = [
        ":comparison_functions",
        "//base:builtins",
        "//common:kind",
        "//internal:testing",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "container_membership_functions",
    srcs = [
        "container_membership_functions.cc",
    ],
    hdrs = [
        "container_membership_functions.h",
    ],
    deps = [
        "//base:builtins",
        "//base:function_adapter",
        "//common:value",
        "//internal:number",
        "//internal:status_macros",
        "//runtime:function_registry",
        "//runtime:register_function_helper",
        "//runtime:runtime_options",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "container_membership_functions_test",
    size = "small",
    srcs = [
        "container_membership_functions_test.cc",
    ],
    deps = [
        ":container_membership_functions",
        "//base:builtins",
        "//common:function_descriptor",
        "//common:kind",
        "//internal:testing",
        "//runtime:function_registry",
        "//runtime:runtime_options",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "equality_functions",
    srcs = ["equality_functions.cc"],
    hdrs = ["equality_functions.h"],
    deps = [
        "//base:builtins",
        "//base:function_adapter",
        "//common:value",
        "//common:value_kind",
        "//internal:number",
        "//internal:status_macros",
        "//runtime:function_registry",
        "//runtime:register_function_helper",
        "//runtime:runtime_options",
        "//runtime/internal:errors",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/functional:function_ref",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:optional",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "equality_functions_test",
    size = "small",
    srcs = [
        "equality_functions_test.cc",
    ],
    deps = [
        ":equality_functions",
        "//base:builtins",
        "//common:function_descriptor",
        "//common:kind",
        "//internal:testing",
        "//runtime:function_registry",
        "//runtime:runtime_options",
        "@com_google_absl//absl/status:status_matchers",
    ],
)

cc_library(
    name = "logical_functions",
    srcs = [
        "logical_functions.cc",
    ],
    hdrs = [
        "logical_functions.h",
    ],
    deps = [
        "//base:builtins",
        "//base:function_adapter",
        "//common:value",
        "//internal:status_macros",
        "//runtime:function_registry",
        "//runtime:register_function_helper",
        "//runtime:runtime_options",
        "//runtime/internal:errors",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "logical_functions_test",
    size = "small",
    srcs = [
        "logical_functions_test.cc",
    ],
    deps = [
        ":logical_functions",
        "//base:builtins",
        "//common:function_descriptor",
        "//common:kind",
        "//common:value",
        "//internal:testing",
        "//internal:testing_descriptor_pool",
        "//internal:testing_message_factory",
        "//runtime:function",
        "//runtime:function_overload_reference",
        "//runtime:function_registry",
        "//runtime:runtime_options",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "container_functions",
    srcs = ["container_functions.cc"],
    hdrs = ["container_functions.h"],
    deps = [
        "//base:builtins",
        "//base:function_adapter",
        "//common:value",
        "//internal:status_macros",
        "//runtime:function_registry",
        "//runtime:runtime_options",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "container_functions_test",
    size = "small",
    srcs = [
        "container_functions_test.cc",
    ],
    deps = [
        ":container_functions",
        "//base:builtins",
        "//common:function_descriptor",
        "//internal:testing",
    ],
)

cc_library(
    name = "type_conversion_functions",
    srcs = ["type_conversion_functions.cc"],
    hdrs = ["type_conversion_functions.h"],
    deps = [
        "//base:builtins",
        "//base:function_adapter",
        "//common:value",
        "//internal:overflow",
        "//internal:status_macros",
        "//internal:time",
        "//internal:utf8",
        "//runtime:function_registry",
        "//runtime:runtime_options",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
    ],
)

cc_test(
    name = "type_conversion_functions_test",
    size = "small",
    srcs = [
        "type_conversion_functions_test.cc",
    ],
    deps = [
        ":type_conversion_functions",
        "//base:builtins",
        "//common:function_descriptor",
        "//internal:testing",
    ],
)

cc_library(
    name = "arithmetic_functions",
    srcs = ["arithmetic_functions.cc"],
    hdrs = ["arithmetic_functions.h"],
    deps = [
        "//base:builtins",
        "//base:function_adapter",
        "//common:value",
        "//internal:overflow",
        "//internal:status_macros",
        "//runtime:function_registry",
        "//runtime:runtime_options",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "arithmetic_functions_test",
    size = "small",
    srcs = [
        "arithmetic_functions_test.cc",
    ],
    deps = [
        ":arithmetic_functions",
        "//base:builtins",
        "//common:function_descriptor",
        "//internal:testing",
    ],
)

cc_library(
    name = "time_functions",
    srcs = ["time_functions.cc"],
    hdrs = ["time_functions.h"],
    deps = [
        "//base:builtins",
        "//base:function_adapter",
        "//common:value",
        "//internal:overflow",
        "//internal:status_macros",
        "//runtime:function_registry",
        "//runtime:runtime_options",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
    ],
)

cc_test(
    name = "time_functions_test",
    size = "small",
    srcs = [
        "time_functions_test.cc",
    ],
    deps = [
        ":time_functions",
        "//base:builtins",
        "//common:function_descriptor",
        "//internal:testing",
    ],
)

cc_library(
    name = "string_functions",
    srcs = ["string_functions.cc"],
    hdrs = ["string_functions.h"],
    deps = [
        "//base:builtins",
        "//base:function_adapter",
        "//common:value",
        "//internal:status_macros",
        "//runtime:function_registry",
        "//runtime:runtime_options",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "string_functions_test",
    size = "small",
    srcs = [
        "string_functions_test.cc",
    ],
    deps = [
        ":string_functions",
        "//base:builtins",
        "//common:function_descriptor",
        "//internal:testing",
    ],
)

cc_library(
    name = "regex_functions",
    srcs = ["regex_functions.cc"],
    hdrs = ["regex_functions.h"],
    deps = [
        "//base:builtins",
        "//base:function_adapter",
        "//common:value",
        "//internal:status_macros",
        "//runtime:function_registry",
        "//runtime:runtime_options",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_googlesource_code_re2//:re2",
    ],
)

cc_test(
    name = "regex_functions_test",
    srcs = ["regex_functions_test.cc"],
    deps = [
        ":regex_functions",
        "//base:builtins",
        "//common:function_descriptor",
        "//internal:testing",
    ],
)
