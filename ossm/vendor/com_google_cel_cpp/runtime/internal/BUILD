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

package(
    # Internals for cel/runtime.
    default_visibility = ["//visibility:public"],
)

licenses(["notice"])

cc_library(
    name = "runtime_friend_access",
    hdrs = ["runtime_friend_access.h"],
    deps = [
        "//common:native_type",
        "//runtime",
        "//runtime:runtime_builder",
    ],
)

cc_library(
    name = "runtime_env",
    srcs = ["runtime_env.cc"],
    hdrs = ["runtime_env.h"],
    deps = [
        "//eval/public:cel_function_registry",
        "//eval/public:cel_type_registry",
        "//internal:noop_delete",
        "//internal:well_known_types",
        "//runtime:function_registry",
        "//runtime:type_registry",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/synchronization",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "runtime_impl",
    srcs = ["runtime_impl.cc"],
    hdrs = ["runtime_impl.h"],
    deps = [
        ":runtime_env",
        "//base:ast",
        "//base:data",
        "//common:native_type",
        "//common:value",
        "//eval/compiler:flat_expr_builder",
        "//eval/eval:attribute_trail",
        "//eval/eval:comprehension_slots",
        "//eval/eval:direct_expression_step",
        "//eval/eval:evaluator_core",
        "//internal:casts",
        "//internal:status_macros",
        "//internal:well_known_types",
        "//runtime",
        "//runtime:activation_interface",
        "//runtime:function_registry",
        "//runtime:runtime_options",
        "//runtime:type_registry",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/log:absl_check",
        "@com_google_absl//absl/status:statusor",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "convert_constant",
    srcs = ["convert_constant.cc"],
    hdrs = ["convert_constant.h"],
    deps = [
        "//common:allocator",
        "//common:constant",
        "//common:value",
        "//common/ast:expr",
        "//eval/internal:errors",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:variant",
    ],
)

cc_library(
    name = "errors",
    srcs = ["errors.cc"],
    hdrs = ["errors.h"],
    deps = [
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:cord",
        "@com_google_absl//absl/time",
    ],
)

cc_library(
    name = "issue_collector",
    hdrs = ["issue_collector.h"],
    deps = [
        "//runtime:runtime_issue",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/types:span",
    ],
)

cc_test(
    name = "issue_collector_test",
    srcs = ["issue_collector_test.cc"],
    deps = [
        ":issue_collector",
        "//internal:testing",
        "//runtime:runtime_issue",
        "@com_google_absl//absl/status",
    ],
)

cc_library(
    name = "function_adapter",
    hdrs = [
        "function_adapter.h",
    ],
    deps = [
        "//common:casting",
        "//common:kind",
        "//common:value",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/time",
    ],
)

cc_test(
    name = "function_adapter_test",
    srcs = ["function_adapter_test.cc"],
    deps = [
        ":function_adapter",
        "//common:casting",
        "//common:kind",
        "//common:value",
        "//internal:testing",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/time",
    ],
)

cc_library(
    name = "runtime_env_testing",
    testonly = True,
    srcs = ["runtime_env_testing.cc"],
    hdrs = ["runtime_env_testing.h"],
    deps = [
        ":runtime_env",
        "//internal:noop_delete",
        "//internal:testing_descriptor_pool",
        "//internal:testing_message_factory",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/log:absl_check",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "legacy_runtime_type_provider",
    hdrs = ["legacy_runtime_type_provider.h"],
    deps = [
        "//eval/public/structs:protobuf_descriptor_type_provider",
        "@com_google_absl//absl/base:nullability",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "runtime_type_provider",
    srcs = ["runtime_type_provider.cc"],
    hdrs = ["runtime_type_provider.h"],
    deps = [
        "//common:type",
        "//common:value",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/types:optional",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "attribute_matcher",
    hdrs = ["attribute_matcher.h"],
    deps = ["//base:attributes"],
)

cc_library(
    name = "activation_attribute_matcher_access",
    srcs = ["activation_attribute_matcher_access.cc"],
    hdrs = ["activation_attribute_matcher_access.h"],
    deps = [
        ":attribute_matcher",
        "//eval/public:activation",
        "//runtime:activation",
        "@com_google_absl//absl/base:nullability",
    ],
)
