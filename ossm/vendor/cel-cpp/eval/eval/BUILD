# Copyright 2017 Google LLC
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

# This package contains implementation of expression evaluator
# internals.
package(default_visibility = ["//visibility:public"])

licenses(["notice"])

exports_files(["LICENSE"])

package_group(
    name = "internal_eval_visibility",
    packages = [
        "//eval/...",
        "//extensions",
        "//runtime/internal",
    ],
)

cc_library(
    name = "evaluator_core",
    srcs = [
        "evaluator_core.cc",
    ],
    hdrs = [
        "evaluator_core.h",
    ],
    deps = [
        ":attribute_utility",
        ":comprehension_slots",
        ":evaluator_stack",
        ":iterator_stack",
        "//base:data",
        "//common:native_type",
        "//common:value",
        "//runtime",
        "//runtime:activation_interface",
        "//runtime:runtime_options",
        "//runtime/internal:activation_attribute_matcher_access",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/log:absl_check",
        "@com_google_absl//absl/log:absl_log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "cel_expression_flat_impl",
    srcs = [
        "cel_expression_flat_impl.cc",
    ],
    hdrs = [
        "cel_expression_flat_impl.h",
    ],
    deps = [
        ":attribute_trail",
        ":comprehension_slots",
        ":direct_expression_step",
        ":evaluator_core",
        "//common:native_type",
        "//common:value",
        "//eval/internal:adapter_activation_impl",
        "//eval/internal:interop",
        "//eval/public:base_activation",
        "//eval/public:cel_expression",
        "//eval/public:cel_value",
        "//internal:casts",
        "//internal:status_macros",
        "//runtime/internal:runtime_env",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "comprehension_slots",
    hdrs = [
        "comprehension_slots.h",
    ],
    deps = [
        ":attribute_trail",
        "//common:value",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/base:no_destructor",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/container:fixed_array",
        "@com_google_absl//absl/log:absl_check",
        "@com_google_absl//absl/types:optional",
    ],
)

cc_test(
    name = "comprehension_slots_test",
    srcs = [
        "comprehension_slots_test.cc",
    ],
    deps = [
        ":attribute_trail",
        ":comprehension_slots",
        "//base:attributes",
        "//base:data",
        "//common:memory",
        "//common:value",
        "//internal:testing",
    ],
)

cc_library(
    name = "evaluator_stack",
    srcs = [
        "evaluator_stack.cc",
    ],
    hdrs = [
        "evaluator_stack.h",
    ],
    deps = [
        ":attribute_trail",
        "//common:value",
        "//internal:align",
        "//internal:new",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/base:dynamic_annotations",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/log:absl_check",
        "@com_google_absl//absl/log:absl_log",
        "@com_google_absl//absl/meta:type_traits",
        "@com_google_absl//absl/types:optional",
        "@com_google_absl//absl/types:span",
    ],
)

cc_test(
    name = "evaluator_stack_test",
    srcs = [
        "evaluator_stack_test.cc",
    ],
    deps = [
        ":evaluator_stack",
        "//base:attributes",
        "//common:value",
        "//internal:testing",
    ],
)

cc_library(
    name = "expression_step_base",
    hdrs = [
        "expression_step_base.h",
    ],
    deps = [":evaluator_core"],
)

cc_library(
    name = "const_value_step",
    srcs = [
        "const_value_step.cc",
    ],
    hdrs = [
        "const_value_step.h",
    ],
    deps = [
        ":compiler_constant_step",
        ":direct_expression_step",
        ":evaluator_core",
        "//common:allocator",
        "//common:constant",
        "//common:value",
        "//internal:status_macros",
        "//runtime/internal:convert_constant",
        "@com_google_absl//absl/status:statusor",
    ],
)

cc_library(
    name = "container_access_step",
    srcs = [
        "container_access_step.cc",
    ],
    hdrs = [
        "container_access_step.h",
    ],
    deps = [
        ":attribute_trail",
        ":attribute_utility",
        ":direct_expression_step",
        ":evaluator_core",
        ":expression_step_base",
        "//base:attributes",
        "//common:casting",
        "//common:expr",
        "//common:kind",
        "//common:value",
        "//common:value_kind",
        "//eval/internal:errors",
        "//internal:number",
        "//internal:status_macros",
        "//runtime/internal:errors",
        "@com_google_absl//absl/log:absl_check",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:optional",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "regex_match_step",
    srcs = ["regex_match_step.cc"],
    hdrs = ["regex_match_step.h"],
    deps = [
        ":attribute_trail",
        ":direct_expression_step",
        ":evaluator_core",
        ":expression_step_base",
        "//common:value",
        "//internal:status_macros",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings:cord",
        "@com_google_absl//absl/strings:string_view",
        "@com_googlesource_code_re2//:re2",
    ],
)

cc_library(
    name = "ident_step",
    srcs = [
        "ident_step.cc",
    ],
    hdrs = [
        "ident_step.h",
    ],
    deps = [
        ":attribute_trail",
        ":comprehension_slots",
        ":direct_expression_step",
        ":evaluator_core",
        ":expression_step_base",
        "//common:expr",
        "//common:value",
        "//eval/internal:errors",
        "//internal:status_macros",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "function_step",
    srcs = [
        "function_step.cc",
    ],
    hdrs = [
        "function_step.h",
    ],
    deps = [
        ":attribute_trail",
        ":direct_expression_step",
        ":evaluator_core",
        ":expression_step_base",
        "//common:casting",
        "//common:expr",
        "//common:function_descriptor",
        "//common:kind",
        "//common:value",
        "//common:value_kind",
        "//eval/internal:errors",
        "//internal:status_macros",
        "//runtime:activation_interface",
        "//runtime:function",
        "//runtime:function_overload_reference",
        "//runtime:function_provider",
        "//runtime:function_registry",
        "@com_google_absl//absl/container:inlined_vector",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:optional",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "select_step",
    srcs = [
        "select_step.cc",
    ],
    hdrs = [
        "select_step.h",
    ],
    deps = [
        ":attribute_trail",
        ":direct_expression_step",
        ":evaluator_core",
        ":expression_step_base",
        "//common:expr",
        "//common:value",
        "//common:value_kind",
        "//eval/internal:errors",
        "//internal:status_macros",
        "//runtime:runtime_options",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/log:absl_check",
        "@com_google_absl//absl/log:absl_log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:optional",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "create_list_step",
    srcs = [
        "create_list_step.cc",
    ],
    hdrs = [
        "create_list_step.h",
    ],
    deps = [
        ":attribute_trail",
        ":attribute_utility",
        ":direct_expression_step",
        ":evaluator_core",
        ":expression_step_base",
        "//common:casting",
        "//common:expr",
        "//common:value",
        "//internal:status_macros",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/types:optional",
    ],
)

cc_library(
    name = "create_struct_step",
    srcs = [
        "create_struct_step.cc",
    ],
    hdrs = [
        "create_struct_step.h",
    ],
    deps = [
        ":attribute_trail",
        ":direct_expression_step",
        ":evaluator_core",
        ":expression_step_base",
        "//common:casting",
        "//common:value",
        "//internal:status_macros",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:optional",
    ],
)

cc_library(
    name = "create_map_step",
    srcs = [
        "create_map_step.cc",
    ],
    hdrs = [
        "create_map_step.h",
    ],
    deps = [
        ":attribute_trail",
        ":direct_expression_step",
        ":evaluator_core",
        ":expression_step_base",
        "//common:casting",
        "//common:value",
        "//internal:status_macros",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:optional",
    ],
)

cc_library(
    name = "jump_step",
    srcs = [
        "jump_step.cc",
    ],
    hdrs = [
        "jump_step.h",
    ],
    deps = [
        ":evaluator_core",
        ":expression_step_base",
        "//common:value",
        "//eval/internal:errors",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/types:optional",
        "@com_google_cel_spec//proto/cel/expr:syntax_cc_proto",
    ],
)

cc_library(
    name = "logic_step",
    srcs = [
        "logic_step.cc",
    ],
    hdrs = [
        "logic_step.h",
    ],
    deps = [
        ":attribute_trail",
        ":direct_expression_step",
        ":evaluator_core",
        ":expression_step_base",
        "//base:builtins",
        "//common:casting",
        "//common:value",
        "//common:value_kind",
        "//eval/internal:errors",
        "//internal:status_macros",
        "//runtime/internal:errors",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/types:optional",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "equality_steps",
    srcs = [
        "equality_steps.cc",
    ],
    hdrs = [
        "equality_steps.h",
    ],
    deps = [
        ":attribute_trail",
        ":direct_expression_step",
        ":evaluator_core",
        ":expression_step_base",
        "//base:builtins",
        "//common:value",
        "//common:value_kind",
        "//internal:number",
        "//internal:status_macros",
        "//runtime/internal:errors",
        "//runtime/standard:equality_functions",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
    ],
)

cc_test(
    name = "equality_steps_test",
    srcs = [
        "equality_steps_test.cc",
    ],
    deps = [
        ":attribute_trail",
        ":direct_expression_step",
        ":equality_steps",
        ":evaluator_core",
        "//base:attributes",
        "//common:value",
        "//common:value_kind",
        "//common:value_testing",
        "//internal:testing",
        "//internal:testing_descriptor_pool",
        "//internal:testing_message_factory",
        "//runtime:activation",
        "//runtime:runtime_options",
        "//runtime/internal:runtime_type_provider",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/log:absl_check",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "comprehension_step",
    srcs = [
        "comprehension_step.cc",
    ],
    hdrs = [
        "comprehension_step.h",
    ],
    deps = [
        ":attribute_trail",
        ":comprehension_slots",
        ":direct_expression_step",
        ":evaluator_core",
        ":expression_step_base",
        "//base:attributes",
        "//common:casting",
        "//common:value",
        "//common:value_kind",
        "//eval/internal:errors",
        "//internal:status_macros",
        "@com_google_absl//absl/base",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/log:absl_check",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
    ],
)

cc_test(
    name = "comprehension_step_test",
    size = "small",
    srcs = [
        "comprehension_step_test.cc",
    ],
    deps = [
        ":attribute_trail",
        ":cel_expression_flat_impl",
        ":comprehension_slots",
        ":comprehension_step",
        ":const_value_step",
        ":direct_expression_step",
        ":evaluator_core",
        ":expression_step_base",
        ":ident_step",
        "//base:data",
        "//common:expr",
        "//common:value",
        "//common:value_testing",
        "//eval/public:activation",
        "//eval/public:cel_attribute",
        "//eval/public:cel_value",
        "//eval/public/structs:cel_proto_wrapper",
        "//internal:status_macros",
        "//internal:testing",
        "//internal:testing_descriptor_pool",
        "//internal:testing_message_factory",
        "//runtime:activation",
        "//runtime:runtime_options",
        "//runtime/internal:runtime_env_testing",
        "//runtime/internal:runtime_type_provider",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_cel_spec//proto/cel/expr:syntax_cc_proto",
        "@com_google_protobuf//:protobuf",
        "@com_google_protobuf//:struct_cc_proto",
    ],
)

cc_test(
    name = "evaluator_core_test",
    size = "small",
    srcs = [
        "evaluator_core_test.cc",
    ],
    deps = [
        ":cel_expression_flat_impl",
        ":evaluator_core",
        "//base:data",
        "//common:value",
        "//eval/compiler:cel_expression_builder_flat_impl",
        "//eval/internal:interop",
        "//eval/public:activation",
        "//eval/public:builtin_func_registrar",
        "//eval/public:cel_value",
        "//internal:testing",
        "//internal:testing_descriptor_pool",
        "//internal:testing_message_factory",
        "//runtime:activation",
        "//runtime:runtime_options",
        "//runtime/internal:runtime_env_testing",
        "//runtime/internal:runtime_type_provider",
        "@com_google_absl//absl/status",
        "@com_google_cel_spec//proto/cel/expr:syntax_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "const_value_step_test",
    size = "small",
    srcs = [
        "const_value_step_test.cc",
    ],
    deps = [
        ":cel_expression_flat_impl",
        ":const_value_step",
        ":evaluator_core",
        "//base:data",
        "//common:constant",
        "//common:expr",
        "//eval/internal:errors",
        "//eval/public:activation",
        "//eval/public:cel_value",
        "//eval/public/testing:matchers",
        "//internal:status_macros",
        "//internal:testing",
        "//runtime:runtime_options",
        "//runtime/internal:runtime_env",
        "//runtime/internal:runtime_env_testing",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/time",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "container_access_step_test",
    size = "small",
    srcs = [
        "container_access_step_test.cc",
    ],
    deps = [
        ":cel_expression_flat_impl",
        ":container_access_step",
        ":direct_expression_step",
        ":evaluator_core",
        ":ident_step",
        "//base:builtins",
        "//base:data",
        "//common:ast",
        "//common:expr",
        "//eval/public:activation",
        "//eval/public:cel_attribute",
        "//eval/public:cel_expr_builder_factory",
        "//eval/public:cel_expression",
        "//eval/public:cel_options",
        "//eval/public:cel_value",
        "//eval/public:unknown_set",
        "//eval/public/containers:container_backed_list_impl",
        "//eval/public/containers:container_backed_map_impl",
        "//eval/public/structs:cel_proto_wrapper",
        "//eval/public/testing:matchers",
        "//internal:testing",
        "//parser",
        "//runtime/internal:runtime_env",
        "//runtime/internal:runtime_env_testing",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/status",
        "@com_google_cel_spec//proto/cel/expr:syntax_cc_proto",
        "@com_google_protobuf//:protobuf",
        "@com_google_protobuf//:struct_cc_proto",
    ],
)

cc_test(
    name = "regex_match_step_test",
    size = "small",
    srcs = [
        "regex_match_step_test.cc",
    ],
    deps = [
        ":regex_match_step",
        "//eval/public:activation",
        "//eval/public:builtin_func_registrar",
        "//eval/public:cel_expr_builder_factory",
        "//eval/public:cel_options",
        "//internal:testing",
        "//parser",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_cel_spec//proto/cel/expr:checked_cc_proto",
        "@com_google_cel_spec//proto/cel/expr:syntax_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "ident_step_test",
    size = "small",
    srcs = [
        "ident_step_test.cc",
    ],
    deps = [
        ":attribute_trail",
        ":cel_expression_flat_impl",
        ":evaluator_core",
        ":ident_step",
        "//base:data",
        "//common:casting",
        "//common:expr",
        "//common:memory",
        "//common:value",
        "//eval/public:activation",
        "//eval/public:cel_attribute",
        "//internal:testing",
        "//internal:testing_descriptor_pool",
        "//internal:testing_message_factory",
        "//runtime:activation",
        "//runtime:runtime_options",
        "//runtime/internal:runtime_env_testing",
        "//runtime/internal:runtime_type_provider",
        "@com_google_absl//absl/status",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "function_step_test",
    size = "small",
    srcs = [
        "function_step_test.cc",
    ],
    deps = [
        ":cel_expression_flat_impl",
        ":const_value_step",
        ":direct_expression_step",
        ":evaluator_core",
        ":function_step",
        ":ident_step",
        "//base:builtins",
        "//base:data",
        "//common:constant",
        "//common:expr",
        "//common:kind",
        "//common:value",
        "//eval/internal:interop",
        "//eval/public:activation",
        "//eval/public:cel_attribute",
        "//eval/public:cel_function",
        "//eval/public:cel_function_registry",
        "//eval/public:cel_options",
        "//eval/public:cel_value",
        "//eval/public:portable_cel_function_adapter",
        "//eval/public/structs:cel_proto_wrapper",
        "//eval/public/testing:matchers",
        "//eval/testutil:test_message_cc_proto",
        "//internal:testing",
        "//runtime:function_overload_reference",
        "//runtime:function_registry",
        "//runtime:runtime_options",
        "//runtime:standard_functions",
        "//runtime/internal:runtime_env_testing",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "logic_step_test",
    size = "small",
    srcs = [
        "logic_step_test.cc",
    ],
    deps = [
        ":attribute_trail",
        ":cel_expression_flat_impl",
        ":const_value_step",
        ":direct_expression_step",
        ":evaluator_core",
        ":ident_step",
        ":logic_step",
        "//base:attributes",
        "//base:data",
        "//common:casting",
        "//common:expr",
        "//common:unknown",
        "//common:value",
        "//eval/public:activation",
        "//eval/public:cel_attribute",
        "//eval/public:cel_value",
        "//eval/public:unknown_attribute_set",
        "//eval/public:unknown_set",
        "//internal:status_macros",
        "//internal:testing",
        "//internal:testing_descriptor_pool",
        "//internal:testing_message_factory",
        "//runtime:activation",
        "//runtime:runtime_options",
        "//runtime/internal:runtime_env",
        "//runtime/internal:runtime_env_testing",
        "//runtime/internal:runtime_type_provider",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "select_step_test",
    size = "small",
    srcs = [
        "select_step_test.cc",
    ],
    deps = [
        ":attribute_trail",
        ":cel_expression_flat_impl",
        ":const_value_step",
        ":evaluator_core",
        ":ident_step",
        ":select_step",
        "//base:attributes",
        "//base:data",
        "//common:casting",
        "//common:expr",
        "//common:legacy_value",
        "//common:value",
        "//common:value_testing",
        "//eval/public:activation",
        "//eval/public:cel_attribute",
        "//eval/public:cel_value",
        "//eval/public/containers:container_backed_map_impl",
        "//eval/public/structs:cel_proto_wrapper",
        "//eval/public/structs:legacy_type_adapter",
        "//eval/public/structs:trivial_legacy_type_info",
        "//eval/public/testing:matchers",
        "//eval/testutil:test_extensions_cc_proto",
        "//eval/testutil:test_message_cc_proto",
        "//extensions/protobuf:value",
        "//internal:proto_matchers",
        "//internal:status_macros",
        "//internal:testing",
        "//internal:testing_descriptor_pool",
        "//internal:testing_message_factory",
        "//runtime:activation",
        "//runtime:runtime_options",
        "//runtime/internal:runtime_env",
        "//runtime/internal:runtime_env_testing",
        "//runtime/internal:runtime_type_provider",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/log:absl_check",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_cel_spec//proto/cel/expr:syntax_cc_proto",
        "@com_google_cel_spec//proto/cel/expr/conformance/proto3:test_all_types_cc_proto",
        "@com_google_protobuf//:wrappers_cc_proto",
    ],
)

cc_test(
    name = "create_list_step_test",
    size = "small",
    srcs = [
        "create_list_step_test.cc",
    ],
    deps = [
        ":attribute_trail",
        ":cel_expression_flat_impl",
        ":const_value_step",
        ":create_list_step",
        ":direct_expression_step",
        ":evaluator_core",
        ":ident_step",
        "//base:attributes",
        "//base:data",
        "//common:casting",
        "//common:expr",
        "//common:value",
        "//common:value_testing",
        "//eval/internal:interop",
        "//eval/public:activation",
        "//eval/public:cel_attribute",
        "//eval/public:cel_value",
        "//eval/public:unknown_attribute_set",
        "//eval/public/testing:matchers",
        "//internal:status_macros",
        "//internal:testing",
        "//internal:testing_descriptor_pool",
        "//internal:testing_message_factory",
        "//runtime:activation",
        "//runtime:runtime_options",
        "//runtime/internal:runtime_env",
        "//runtime/internal:runtime_env_testing",
        "//runtime/internal:runtime_type_provider",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "create_struct_step_test",
    size = "small",
    srcs = [
        "create_struct_step_test.cc",
    ],
    deps = [
        ":cel_expression_flat_impl",
        ":create_struct_step",
        ":direct_expression_step",
        ":evaluator_core",
        ":ident_step",
        "//base:data",
        "//common:expr",
        "//eval/public:activation",
        "//eval/public:cel_type_registry",
        "//eval/public:cel_value",
        "//eval/public:unknown_set",
        "//eval/public/containers:container_backed_list_impl",
        "//eval/public/containers:container_backed_map_impl",
        "//eval/public/structs:cel_proto_wrapper",
        "//eval/testutil:test_message_cc_proto",
        "//internal:proto_matchers",
        "//internal:status_macros",
        "//internal:testing",
        "//runtime:runtime_options",
        "//runtime/internal:runtime_env",
        "//runtime/internal:runtime_env_testing",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
        "@com_google_cel_spec//proto/cel/expr:syntax_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "create_map_step_test",
    size = "small",
    srcs = [
        "create_map_step_test.cc",
    ],
    deps = [
        ":cel_expression_flat_impl",
        ":create_map_step",
        ":direct_expression_step",
        ":evaluator_core",
        ":ident_step",
        "//base:data",
        "//common:expr",
        "//eval/public:activation",
        "//eval/public:cel_value",
        "//eval/public:unknown_set",
        "//eval/testutil:test_message_cc_proto",
        "//internal:status_macros",
        "//internal:testing",
        "//runtime:runtime_options",
        "//runtime/internal:runtime_env",
        "//runtime/internal:runtime_env_testing",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_cel_spec//proto/cel/expr:syntax_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "attribute_trail",
    srcs = ["attribute_trail.cc"],
    hdrs = ["attribute_trail.h"],
    deps = [
        "//base:attributes",
        "@com_google_absl//absl/types:optional",
        "@com_google_absl//absl/utility",
    ],
)

cc_test(
    name = "attribute_trail_test",
    size = "small",
    srcs = [
        "attribute_trail_test.cc",
    ],
    deps = [
        ":attribute_trail",
        "//eval/public:cel_attribute",
        "//eval/public:cel_value",
        "//internal:testing",
        "@com_google_cel_spec//proto/cel/expr:syntax_cc_proto",
    ],
)

cc_library(
    name = "attribute_utility",
    srcs = ["attribute_utility.cc"],
    hdrs = ["attribute_utility.h"],
    deps = [
        ":attribute_trail",
        "//base:attributes",
        "//base:function_result",
        "//base:function_result_set",
        "//base/internal:unknown_set",
        "//common:casting",
        "//common:function_descriptor",
        "//common:unknown",
        "//common:value",
        "//eval/internal:errors",
        "//internal:status_macros",
        "//runtime/internal:attribute_matcher",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/types:optional",
        "@com_google_absl//absl/types:span",
    ],
)

cc_test(
    name = "attribute_utility_test",
    size = "small",
    srcs = [
        "attribute_utility_test.cc",
    ],
    deps = [
        ":attribute_trail",
        ":attribute_utility",
        "//base:attributes",
        "//common:unknown",
        "//common:value",
        "//eval/public:cel_attribute",
        "//eval/public:cel_value",
        "//eval/public:unknown_attribute_set",
        "//eval/public:unknown_set",
        "//internal:testing",
        "//runtime/internal:attribute_matcher",
        "@com_google_absl//absl/types:span",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "ternary_step",
    srcs = [
        "ternary_step.cc",
    ],
    hdrs = [
        "ternary_step.h",
    ],
    deps = [
        ":attribute_trail",
        ":direct_expression_step",
        ":evaluator_core",
        ":expression_step_base",
        "//base:builtins",
        "//common:value",
        "//eval/internal:errors",
        "//internal:status_macros",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
    ],
)

cc_test(
    name = "ternary_step_test",
    size = "small",
    srcs = [
        "ternary_step_test.cc",
    ],
    deps = [
        ":attribute_trail",
        ":cel_expression_flat_impl",
        ":const_value_step",
        ":direct_expression_step",
        ":evaluator_core",
        ":ident_step",
        ":ternary_step",
        "//base:attributes",
        "//base:data",
        "//common:casting",
        "//common:expr",
        "//common:value",
        "//eval/public:activation",
        "//eval/public:cel_value",
        "//eval/public:unknown_attribute_set",
        "//eval/public:unknown_set",
        "//internal:status_macros",
        "//internal:testing",
        "//internal:testing_descriptor_pool",
        "//internal:testing_message_factory",
        "//runtime:activation",
        "//runtime:runtime_options",
        "//runtime/internal:runtime_env",
        "//runtime/internal:runtime_env_testing",
        "//runtime/internal:runtime_type_provider",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/status",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "shadowable_value_step",
    srcs = ["shadowable_value_step.cc"],
    hdrs = ["shadowable_value_step.h"],
    deps = [
        ":attribute_trail",
        ":direct_expression_step",
        ":evaluator_core",
        ":expression_step_base",
        "//common:value",
        "//internal:status_macros",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
    ],
)

cc_test(
    name = "shadowable_value_step_test",
    size = "small",
    srcs = ["shadowable_value_step_test.cc"],
    deps = [
        ":cel_expression_flat_impl",
        ":evaluator_core",
        ":shadowable_value_step",
        "//base:data",
        "//common:value",
        "//eval/internal:interop",
        "//eval/public:activation",
        "//eval/public:cel_value",
        "//internal:status_macros",
        "//internal:testing",
        "//runtime:runtime_options",
        "//runtime/internal:runtime_env",
        "//runtime/internal:runtime_env_testing",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/status:statusor",
    ],
)

cc_library(
    name = "compiler_constant_step",
    srcs = ["compiler_constant_step.cc"],
    hdrs = ["compiler_constant_step.h"],
    deps = [
        ":attribute_trail",
        ":direct_expression_step",
        ":evaluator_core",
        ":expression_step_base",
        "//common:native_type",
        "//common:value",
        "@com_google_absl//absl/status",
    ],
)

cc_test(
    name = "compiler_constant_step_test",
    srcs = ["compiler_constant_step_test.cc"],
    deps = [
        ":compiler_constant_step",
        ":evaluator_core",
        "//common:native_type",
        "//common:value",
        "//internal:testing",
        "//internal:testing_descriptor_pool",
        "//internal:testing_message_factory",
        "//runtime:activation",
        "//runtime:runtime_options",
        "//runtime/internal:runtime_type_provider",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "lazy_init_step",
    srcs = ["lazy_init_step.cc"],
    hdrs = ["lazy_init_step.h"],
    deps = [
        ":attribute_trail",
        ":comprehension_slots",
        ":direct_expression_step",
        ":evaluator_core",
        ":expression_step_base",
        "//common:value",
        "//internal:status_macros",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/status",
        "@com_google_cel_spec//proto/cel/expr:value_cc_proto",
    ],
)

cc_test(
    name = "lazy_init_step_test",
    srcs = ["lazy_init_step_test.cc"],
    deps = [
        ":const_value_step",
        ":evaluator_core",
        ":lazy_init_step",
        "//base:data",
        "//common:value",
        "//internal:testing",
        "//internal:testing_descriptor_pool",
        "//internal:testing_message_factory",
        "//runtime:activation",
        "//runtime:runtime_options",
        "//runtime/internal:runtime_type_provider",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "direct_expression_step",
    srcs = ["direct_expression_step.cc"],
    hdrs = ["direct_expression_step.h"],
    deps = [
        ":attribute_trail",
        ":evaluator_core",
        "//common:native_type",
        "//common:value",
        "//internal:status_macros",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/types:optional",
    ],
)

cc_library(
    name = "trace_step",
    hdrs = ["trace_step.h"],
    deps = [
        ":attribute_trail",
        ":direct_expression_step",
        ":evaluator_core",
        "//common:native_type",
        "//common:value",
        "//internal:status_macros",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/types:optional",
    ],
)

cc_library(
    name = "optional_or_step",
    srcs = ["optional_or_step.cc"],
    hdrs = ["optional_or_step.h"],
    deps = [
        ":attribute_trail",
        ":direct_expression_step",
        ":evaluator_core",
        ":expression_step_base",
        ":jump_step",
        "//common:casting",
        "//common:value",
        "//internal:status_macros",
        "//runtime/internal:errors",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/types:optional",
        "@com_google_absl//absl/types:span",
    ],
)

cc_test(
    name = "optional_or_step_test",
    srcs = ["optional_or_step_test.cc"],
    deps = [
        ":attribute_trail",
        ":const_value_step",
        ":direct_expression_step",
        ":evaluator_core",
        ":optional_or_step",
        "//common:casting",
        "//common:value",
        "//common:value_kind",
        "//common:value_testing",
        "//internal:testing",
        "//internal:testing_descriptor_pool",
        "//internal:testing_message_factory",
        "//runtime:activation",
        "//runtime:runtime_options",
        "//runtime/internal:errors",
        "//runtime/internal:runtime_type_provider",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "iterator_stack",
    hdrs = ["iterator_stack.h"],
    deps = [
        "//common:value",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/log:absl_check",
    ],
)
