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

package(
    # Under active development, not yet being released.
    default_visibility = ["//visibility:public"],
)

licenses(["notice"])

cc_library(
    name = "attributes",
    srcs = [
        "attribute.cc",
    ],
    hdrs = [
        "attribute.h",
        "attribute_set.h",
    ],
    deps = [
        ":kind",
        "//internal:status_macros",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:btree",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:optional",
        "@com_google_absl//absl/types:span",
        "@com_google_absl//absl/types:variant",
    ],
)

cc_library(
    name = "kind",
    hdrs = ["kind.h"],
    deps = [
        "//common:kind",
        "//common:type_kind",
        "//common:value_kind",
    ],
)

cc_library(
    name = "operators",
    srcs = ["operators.cc"],
    hdrs = ["operators.h"],
    deps = [
        "//base/internal:operators",
        "@com_google_absl//absl/base",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/log:absl_check",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:optional",
    ],
)

cc_test(
    name = "operators_test",
    srcs = ["operators_test.cc"],
    deps = [
        ":operators",
        "//base/internal:operators",
        "//internal:testing",
        "@com_google_absl//absl/hash:hash_testing",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:optional",
    ],
)

# Build target encompassing cel::Type, cel::Value, and their related classes.
cc_library(
    name = "data",
    hdrs = [
        "type_provider.h",
    ],
    deps = [
        "//common:value",
    ],
)

cc_library(
    name = "function",
    hdrs = [
        "function.h",
    ],
    deps = [
        "//common:value",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "function_descriptor",
    srcs = [
        "function_descriptor.cc",
    ],
    hdrs = [
        "function_descriptor.h",
    ],
    deps = [
        ":kind",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "function_result",
    hdrs = [
        "function_result.h",
    ],
    deps = [":function_descriptor"],
)

cc_library(
    name = "function_result_set",
    srcs = [
        "function_result_set.cc",
    ],
    hdrs = [
        "function_result_set.h",
    ],
    deps = [
        ":function_result",
        "@com_google_absl//absl/container:btree",
    ],
)

cc_library(
    name = "ast",
    hdrs = ["ast.h"],
    deps = ["//common:ast"],
)

cc_library(
    name = "function_adapter",
    hdrs = ["function_adapter.h"],
    deps = [
        "//runtime:function_adapter",
    ],
)

cc_library(
    name = "builtins",
    hdrs = ["builtins.h"],
)
