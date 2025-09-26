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

package(default_visibility = ["//visibility:public"])

package_group(
    name = "ast_visibility",
    packages = [
        "//eval/compiler",
        "//extensions",
    ],
)

licenses(["notice"])

exports_files(["LICENSE"])

cc_library(
    name = "message_wrapper",
    hdrs = [
        "message_wrapper.h",
    ],
    deps = [
        "//base/internal:message_wrapper",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/numeric:bits",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "message_wrapper_test",
    srcs = [
        "message_wrapper_test.cc",
    ],
    deps = [
        ":message_wrapper",
        "//eval/public/structs:trivial_legacy_type_info",
        "//eval/testutil:test_message_cc_proto",
        "//internal:casts",
        "//internal:testing",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "cel_value_internal",
    hdrs = [
        "cel_value_internal.h",
    ],
    deps = [
        ":message_wrapper",
        "//internal:casts",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/numeric:bits",
        "@com_google_absl//absl/types:variant",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "cel_value",
    srcs = [
        "cel_value.cc",
    ],
    hdrs = [
        "cel_value.h",
    ],
    deps = [
        ":cel_value_internal",
        ":message_wrapper",
        ":unknown_set",
        "//common:kind",
        "//common:memory",
        "//common:native_type",
        "//eval/internal:errors",
        "//eval/public/structs:legacy_type_info_apis",
        "//extensions/protobuf:memory_manager",
        "//internal:casts",
        "//internal:status_macros",
        "//internal:utf8",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/base:no_destructor",
        "@com_google_absl//absl/log:absl_log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:optional",
        "@com_google_absl//absl/types:variant",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "cel_attribute",
    srcs = [
        "cel_attribute.cc",
    ],
    hdrs = [
        "cel_attribute.h",
    ],
    deps = [
        ":cel_value",
        "//base:attributes",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/types:optional",
        "@com_google_absl//absl/types:variant",
        "@com_google_cel_spec//proto/cel/expr:syntax_cc_proto",
    ],
)

cc_library(
    name = "cel_value_producer",
    hdrs = [
        "cel_value_producer.h",
    ],
    deps = [":cel_value"],
)

cc_library(
    name = "unknown_attribute_set",
    hdrs = [
        "unknown_attribute_set.h",
    ],
    deps = ["//base:attributes"],
)

cc_library(
    name = "activation",
    srcs = [
        "activation.cc",
    ],
    hdrs = [
        "activation.h",
    ],
    deps = [
        ":base_activation",
        ":cel_attribute",
        ":cel_function",
        ":cel_value",
        ":cel_value_producer",
        "//runtime/internal:attribute_matcher",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:optional",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "activation_bind_helper",
    srcs = [
        "activation_bind_helper.cc",
    ],
    hdrs = [
        "activation_bind_helper.h",
    ],
    deps = [
        ":activation",
        "//eval/public/containers:field_access",
        "//eval/public/containers:field_backed_list_impl",
        "//eval/public/containers:field_backed_map_impl",
        "@com_google_absl//absl/status",
    ],
)

cc_library(
    name = "cel_function",
    srcs = [
        "cel_function.cc",
    ],
    hdrs = [
        "cel_function.h",
    ],
    deps = [
        ":cel_value",
        "//common:function_descriptor",
        "//common:value",
        "//eval/internal:interop",
        "//internal:status_macros",
        "//runtime:function",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/types:span",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "cel_function_adapter_impl",
    hdrs = [
        "cel_function_adapter_impl.h",
    ],
    deps = [
        ":cel_function",
        ":cel_function_registry",
        ":cel_value",
        "//internal:status_macros",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "cel_function_adapter",
    hdrs = [
        "cel_function_adapter.h",
    ],
    deps = [
        ":cel_function_adapter_impl",
        ":cel_value",
        "//eval/public/structs:cel_proto_wrapper",
        "@com_google_absl//absl/status",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "portable_cel_function_adapter",
    hdrs = [
        "portable_cel_function_adapter.h",
    ],
    deps = [":cel_function_adapter"],
)

cc_library(
    name = "cel_builtins",
    hdrs = [
        "cel_builtins.h",
    ],
    deps = [
        "//base:builtins",
    ],
)

cc_library(
    name = "builtin_func_registrar",
    srcs = [
        "builtin_func_registrar.cc",
    ],
    hdrs = [
        "builtin_func_registrar.h",
    ],
    deps = [
        ":cel_function_registry",
        ":cel_options",
        "//internal:status_macros",
        "//runtime:function_registry",
        "//runtime:runtime_options",
        "//runtime/standard:arithmetic_functions",
        "//runtime/standard:comparison_functions",
        "//runtime/standard:container_functions",
        "//runtime/standard:container_membership_functions",
        "//runtime/standard:equality_functions",
        "//runtime/standard:logical_functions",
        "//runtime/standard:regex_functions",
        "//runtime/standard:string_functions",
        "//runtime/standard:time_functions",
        "//runtime/standard:type_conversion_functions",
        "@com_google_absl//absl/status",
    ],
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
        ":cel_function_registry",
        ":cel_options",
        "//runtime:function_registry",
        "//runtime:runtime_options",
        "//runtime/standard:comparison_functions",
        "@com_google_absl//absl/status",
    ],
)

cc_test(
    name = "comparison_functions_test",
    size = "small",
    srcs = [
        "comparison_functions_test.cc",
    ],
    deps = [
        ":activation",
        ":cel_expr_builder_factory",
        ":cel_expression",
        ":cel_function_registry",
        ":cel_options",
        ":cel_value",
        ":comparison_functions",
        "//eval/public/testing:matchers",
        "//internal:status_macros",
        "//internal:testing",
        "//parser",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_cel_spec//proto/cel/expr:syntax_cc_proto",
        "@com_google_googleapis//google/rpc/context:attribute_context_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "equality_function_registrar",
    srcs = [
        "equality_function_registrar.cc",
    ],
    hdrs = [
        "equality_function_registrar.h",
    ],
    deps = [
        ":cel_function_registry",
        ":cel_options",
        "//eval/internal:cel_value_equal",
        "//runtime:runtime_options",
        "//runtime/standard:equality_functions",
        "@com_google_absl//absl/status",
    ],
)

cc_test(
    name = "equality_function_registrar_test",
    size = "small",
    srcs = [
        "equality_function_registrar_test.cc",
    ],
    deps = [
        ":activation",
        ":cel_builtins",
        ":cel_expr_builder_factory",
        ":cel_expression",
        ":cel_function_registry",
        ":cel_options",
        ":cel_value",
        ":equality_function_registrar",
        ":message_wrapper",
        "//eval/public/containers:container_backed_list_impl",
        "//eval/public/containers:container_backed_map_impl",
        "//eval/public/structs:cel_proto_wrapper",
        "//eval/public/structs:trivial_legacy_type_info",
        "//eval/public/testing:matchers",
        "//eval/testutil:test_message_cc_proto",
        "//internal:benchmark",
        "//internal:status_macros",
        "//internal:testing",
        "//parser",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:span",
        "@com_google_absl//absl/types:variant",
        "@com_google_cel_spec//proto/cel/expr:syntax_cc_proto",
        "@com_google_googleapis//google/rpc/context:attribute_context_cc_proto",
        "@com_google_protobuf//:any_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "container_function_registrar",
    srcs = [
        "container_function_registrar.cc",
    ],
    hdrs = [
        "container_function_registrar.h",
    ],
    deps = [
        ":cel_function_registry",
        ":cel_options",
        "//runtime:runtime_options",
        "//runtime/standard:container_functions",
        "@com_google_absl//absl/status",
    ],
)

cc_test(
    name = "container_function_registrar_test",
    size = "small",
    srcs = [
        "container_function_registrar_test.cc",
    ],
    deps = [
        ":activation",
        ":cel_expr_builder_factory",
        ":cel_expression",
        ":cel_value",
        ":container_function_registrar",
        ":equality_function_registrar",
        "//eval/public/containers:container_backed_list_impl",
        "//eval/public/testing:matchers",
        "//internal:testing",
        "//parser",
    ],
)

cc_library(
    name = "logical_function_registrar",
    srcs = [
        "logical_function_registrar.cc",
    ],
    hdrs = [
        "logical_function_registrar.h",
    ],
    deps = [
        ":cel_function_registry",
        ":cel_options",
        "//runtime/standard:logical_functions",
        "@com_google_absl//absl/status",
    ],
)

cc_test(
    name = "logical_function_registrar_test",
    size = "small",
    srcs = [
        "logical_function_registrar_test.cc",
    ],
    deps = [
        ":activation",
        ":cel_expr_builder_factory",
        ":cel_expression",
        ":cel_options",
        ":cel_value",
        ":logical_function_registrar",
        ":portable_cel_function_adapter",
        "//eval/public/testing:matchers",
        "//internal:testing",
        "//parser",
        "@com_google_absl//absl/base:no_destructor",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_cel_spec//proto/cel/expr:syntax_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "extension_func_registrar",
    srcs = [
        "extension_func_registrar.cc",
    ],
    hdrs = [
        "extension_func_registrar.h",
    ],
    deps = [
        ":cel_function",
        ":cel_function_adapter",
        ":cel_function_registry",
        ":cel_value",
        "//eval/public/structs:cel_proto_wrapper",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_googleapis//google/type:timeofday_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "cel_expression",
    hdrs = [
        "cel_expression.h",
    ],
    deps = [
        ":base_activation",
        ":cel_function_registry",
        ":cel_type_registry",
        ":cel_value",
        "//common:legacy_value",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_cel_spec//proto/cel/expr:checked_cc_proto",
        "@com_google_cel_spec//proto/cel/expr:syntax_cc_proto",
    ],
)

cc_library(
    name = "source_position",
    srcs = ["source_position.cc"],
    hdrs = ["source_position.h"],
    deps = [
        "@com_google_cel_spec//proto/cel/expr:syntax_cc_proto",
    ],
)

cc_library(
    name = "ast_visitor",
    hdrs = [
        "ast_visitor.h",
    ],
    deps = [
        ":source_position",
        "@com_google_cel_spec//proto/cel/expr:syntax_cc_proto",
    ],
)

cc_library(
    name = "ast_visitor_base",
    hdrs = [
        "ast_visitor_base.h",
    ],
    deps = [
        ":ast_visitor",
        "@com_google_cel_spec//proto/cel/expr:syntax_cc_proto",
    ],
)

cc_library(
    name = "ast_traverse",
    srcs = [
        "ast_traverse.cc",
    ],
    hdrs = [
        "ast_traverse.h",
    ],
    deps = [
        ":ast_visitor",
        ":source_position",
        "@com_google_absl//absl/log:absl_log",
        "@com_google_absl//absl/types:variant",
        "@com_google_cel_spec//proto/cel/expr:syntax_cc_proto",
    ],
)

cc_library(
    name = "cel_options",
    srcs = [
        "cel_options.cc",
    ],
    hdrs = [
        "cel_options.h",
    ],
    deps = [
        "//runtime:runtime_options",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "cel_expr_builder_factory",
    srcs = [
        "cel_expr_builder_factory.cc",
    ],
    hdrs = [
        "cel_expr_builder_factory.h",
    ],
    deps = [
        ":cel_expression",
        ":cel_function",
        ":cel_options",
        "//common:kind",
        "//common:memory",
        "//eval/compiler:cel_expression_builder_flat_impl",
        "//eval/compiler:comprehension_vulnerability_check",
        "//eval/compiler:constant_folding",
        "//eval/compiler:flat_expr_builder",
        "//eval/compiler:qualified_reference_resolver",
        "//eval/compiler:regex_precompilation_optimization",
        "//extensions:select_optimization",
        "//internal:noop_delete",
        "//runtime:runtime_options",
        "//runtime/internal:runtime_env",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/log:absl_log",
        "@com_google_absl//absl/status",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "value_export_util",
    srcs = [
        "value_export_util.cc",
    ],
    hdrs = [
        "value_export_util.h",
    ],
    deps = [
        ":cel_value",
        "//internal:proto_time_encoding",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_protobuf//:json_util",
        "@com_google_protobuf//:protobuf",
        "@com_google_protobuf//:struct_cc_proto",
        "@com_google_protobuf//:time_util",
    ],
)

cc_library(
    name = "cel_function_registry",
    srcs = ["cel_function_registry.cc"],
    hdrs = ["cel_function_registry.h"],
    deps = [
        ":cel_function",
        ":cel_options",
        ":cel_value",
        "//common:function_descriptor",
        "//common:kind",
        "//common:value",
        "//eval/internal:interop",
        "//internal:status_macros",
        "//runtime:function",
        "//runtime:function_overload_reference",
        "//runtime:function_registry",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:node_hash_map",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/types:span",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "cel_value_test",
    size = "small",
    srcs = [
        "cel_value_test.cc",
    ],
    deps = [
        ":cel_value",
        ":unknown_set",
        "//common:memory",
        "//eval/internal:errors",
        "//eval/public/structs:trivial_legacy_type_info",
        "//eval/public/testing:matchers",
        "//eval/testutil:test_message_cc_proto",
        "//extensions/protobuf:memory_manager",
        "//internal:testing",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:cord",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:optional",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "cel_attribute_test",
    size = "small",
    srcs = [
        "cel_attribute_test.cc",
    ],
    deps = [
        ":cel_attribute",
        ":cel_value",
        "//eval/public/structs:cel_proto_wrapper",
        "//internal:status_macros",
        "//internal:testing",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "activation_test",
    size = "small",
    srcs = [
        "activation_test.cc",
    ],
    deps = [
        ":activation",
        ":cel_attribute",
        ":cel_function",
        "//eval/eval:attribute_trail",
        "//eval/eval:ident_step",
        "//extensions/protobuf:memory_manager",
        "//internal:status_macros",
        "//internal:testing",
        "//parser",
    ],
)

cc_test(
    name = "ast_traverse_test",
    srcs = [
        "ast_traverse_test.cc",
    ],
    deps = [
        ":ast_traverse",
        ":ast_visitor",
        "//internal:testing",
    ],
)

cc_library(
    name = "ast_rewrite",
    srcs = [
        "ast_rewrite.cc",
    ],
    hdrs = [
        "ast_rewrite.h",
    ],
    deps = [
        ":ast_visitor",
        ":source_position",
        "@com_google_absl//absl/log:absl_log",
        "@com_google_absl//absl/types:span",
        "@com_google_absl//absl/types:variant",
        "@com_google_cel_spec//proto/cel/expr:syntax_cc_proto",
    ],
)

cc_test(
    name = "ast_rewrite_test",
    srcs = [
        "ast_rewrite_test.cc",
    ],
    deps = [
        ":ast_rewrite",
        ":ast_visitor",
        ":source_position",
        "//internal:testing",
        "//parser",
        "//testutil:util",
        "@com_google_cel_spec//proto/cel/expr:syntax_cc_proto",
    ],
)

cc_test(
    name = "activation_bind_helper_test",
    size = "small",
    srcs = [
        "activation_bind_helper_test.cc",
    ],
    deps = [
        ":activation",
        ":activation_bind_helper",
        "//eval/testutil:test_message_cc_proto",
        "//internal:status_macros",
        "//internal:testing",
        "//testutil:util",
        "@com_google_absl//absl/status",
    ],
)

cc_test(
    name = "cel_function_registry_test",
    srcs = [
        "cel_function_registry_test.cc",
    ],
    deps = [
        ":activation",
        ":cel_function",
        ":cel_function_registry",
        "//common:kind",
        "//eval/internal:adapter_activation_impl",
        "//internal:testing",
        "//runtime:function_overload_reference",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
    ],
)

cc_test(
    name = "cel_function_adapter_test",
    size = "small",
    srcs = [
        "cel_function_adapter_test.cc",
    ],
    deps = [
        ":cel_function_adapter",
        "//internal:status_macros",
        "//internal:testing",
    ],
)

cc_library(
    name = "cel_type_registry",
    srcs = ["cel_type_registry.cc"],
    hdrs = ["cel_type_registry.h"],
    deps = [
        "//base:data",
        "//eval/public/structs:legacy_type_adapter",
        "//eval/public/structs:legacy_type_info_apis",
        "//eval/public/structs:legacy_type_provider",
        "//eval/public/structs:protobuf_descriptor_type_provider",
        "//runtime:type_registry",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:optional",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "cel_type_registry_test",
    srcs = ["cel_type_registry_test.cc"],
    deps = [
        ":cel_type_registry",
        "//base:data",
        "//common:memory",
        "//common:type",
        "//eval/public/structs:legacy_type_adapter",
        "//eval/public/structs:legacy_type_provider",
        "//internal:testing",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:optional",
    ],
)

cc_test(
    name = "cel_type_registry_protobuf_reflection_test",
    srcs = ["cel_type_registry_protobuf_reflection_test.cc"],
    deps = [
        ":cel_type_registry",
        "//common:memory",
        "//common:type",
        "//eval/testutil:test_message_cc_proto",
        "//internal:testing",
        "@com_google_absl//absl/types:optional",
        "@com_google_protobuf//:protobuf",
        "@com_google_protobuf//:struct_cc_proto",
    ],
)

cc_test(
    name = "builtin_func_test",
    size = "small",
    srcs = [
        "builtin_func_test.cc",
    ],
    deps = [
        ":activation",
        ":builtin_func_registrar",
        ":cel_builtins",
        ":cel_expr_builder_factory",
        ":cel_function_registry",
        ":cel_options",
        ":cel_value",
        "//eval/public/structs:cel_proto_wrapper",
        "//internal:status_macros",
        "//internal:testing",
        "//internal:time",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:optional",
        "@com_google_cel_spec//proto/cel/expr:syntax_cc_proto",
    ],
)

cc_test(
    name = "extension_func_test",
    size = "small",
    srcs = [
        "extension_func_test.cc",
    ],
    deps = [
        ":builtin_func_registrar",
        ":cel_expr_builder_factory",
        ":cel_expression",
        ":cel_function_registry",
        ":cel_value",
        ":extension_func_registrar",
        "//eval/public/structs:cel_proto_wrapper",
        "//internal:status_macros",
        "//internal:testing",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:span",
        "@com_google_googleapis//google/type:timeofday_cc_proto",
        "@com_google_protobuf//:protobuf",
        "@com_google_protobuf//:time_util",
    ],
)

cc_test(
    name = "source_position_test",
    size = "small",
    srcs = [
        "source_position_test.cc",
    ],
    deps = [
        ":source_position",
        "//internal:testing",
        "@com_google_cel_spec//proto/cel/expr:syntax_cc_proto",
    ],
)

cc_test(
    name = "unknown_attribute_set_test",
    size = "small",
    srcs = [
        "unknown_attribute_set_test.cc",
    ],
    deps = [
        ":cel_attribute",
        ":cel_value",
        ":unknown_attribute_set",
        "//internal:testing",
    ],
)

cc_test(
    name = "value_export_util_test",
    size = "small",
    srcs = [
        "value_export_util_test.cc",
    ],
    deps = [
        ":value_export_util",
        "//eval/public/containers:container_backed_list_impl",
        "//eval/public/containers:container_backed_map_impl",
        "//eval/public/structs:cel_proto_wrapper",
        "//eval/testutil:test_message_cc_proto",
        "//internal:status_macros",
        "//internal:testing",
        "//testutil:util",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "unknown_function_result_set",
    srcs = ["unknown_function_result_set.cc"],
    hdrs = ["unknown_function_result_set.h"],
    deps = [
        "//base:function_result",
        "//base:function_result_set",
    ],
)

cc_test(
    name = "unknown_function_result_set_test",
    size = "small",
    srcs = [
        "unknown_function_result_set_test.cc",
    ],
    deps = [
        ":cel_function",
        ":cel_value",
        ":unknown_function_result_set",
        "//eval/public/containers:container_backed_list_impl",
        "//eval/public/containers:container_backed_map_impl",
        "//eval/public/structs:cel_proto_wrapper",
        "//internal:testing",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:span",
        "@com_google_protobuf//:duration_cc_proto",
        "@com_google_protobuf//:empty_cc_proto",
        "@com_google_protobuf//:protobuf",
        "@com_google_protobuf//:struct_cc_proto",
        "@com_google_protobuf//:timestamp_cc_proto",
    ],
)

cc_library(
    name = "unknown_set",
    hdrs = ["unknown_set.h"],
    deps = [
        ":unknown_attribute_set",
        ":unknown_function_result_set",
        "//base/internal:unknown_set",
    ],
)

cc_test(
    name = "unknown_set_test",
    srcs = ["unknown_set_test.cc"],
    deps = [
        ":cel_attribute",
        ":cel_function",
        ":unknown_attribute_set",
        ":unknown_function_result_set",
        ":unknown_set",
        "//internal:testing",
        "@com_google_cel_spec//proto/cel/expr:syntax_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "transform_utility",
    srcs = [
        "transform_utility.cc",
    ],
    hdrs = [
        "transform_utility.h",
    ],
    deps = [
        ":cel_value",
        "//eval/public/containers:container_backed_list_impl",
        "//eval/public/containers:container_backed_map_impl",
        "//eval/public/structs:cel_proto_wrapper",
        "//internal:proto_time_encoding",
        "//internal:status_macros",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_cel_spec//proto/cel/expr:value_cc_proto",
        "@com_google_protobuf//:any_cc_proto",
        "@com_google_protobuf//:differencer",
        "@com_google_protobuf//:protobuf",
        "@com_google_protobuf//:struct_cc_proto",
    ],
)

cc_library(
    name = "set_util",
    srcs = ["set_util.cc"],
    hdrs = ["set_util.h"],
    deps = [":cel_value"],
)

cc_library(
    name = "base_activation",
    hdrs = ["base_activation.h"],
    deps = [
        ":cel_attribute",
        ":cel_function",
        ":cel_value",
        "//runtime/internal:attribute_matcher",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/strings",
        "@com_google_protobuf//:field_mask_cc_proto",
    ],
)

cc_test(
    name = "set_util_test",
    size = "small",
    srcs = [
        "set_util_test.cc",
    ],
    deps = [
        ":cel_value",
        ":set_util",
        ":unknown_set",
        "//eval/public/containers:container_backed_list_impl",
        "//eval/public/containers:container_backed_map_impl",
        "//eval/public/structs:cel_proto_wrapper",
        "//internal:testing",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/time",
        "@com_google_protobuf//:empty_cc_proto",
        "@com_google_protobuf//:protobuf",
        "@com_google_protobuf//:struct_cc_proto",
    ],
)

cc_test(
    name = "builtin_func_registrar_test",
    srcs = ["builtin_func_registrar_test.cc"],
    deps = [
        ":activation",
        ":builtin_func_registrar",
        ":cel_expr_builder_factory",
        ":cel_expression",
        ":cel_options",
        ":cel_value",
        "//eval/public/testing:matchers",
        "//internal:testing",
        "//internal:time",
        "//parser",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_cel_spec//proto/cel/expr:syntax_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "cel_number",
    srcs = ["cel_number.cc"],
    hdrs = ["cel_number.h"],
    deps = [
        ":cel_value",
        "//internal:number",
        "@com_google_absl//absl/types:optional",
    ],
)

cc_test(
    name = "cel_number_test",
    srcs = ["cel_number_test.cc"],
    deps = [
        ":cel_number",
        ":cel_value",
        "//internal:testing",
        "@com_google_absl//absl/types:optional",
    ],
)

cc_library(
    name = "string_extension_func_registrar",
    srcs = ["string_extension_func_registrar.cc"],
    hdrs = ["string_extension_func_registrar.h"],
    deps = [
        ":cel_function_registry",
        ":cel_options",
        "//extensions:strings",
        "@com_google_absl//absl/status",
    ],
)

cc_test(
    name = "string_extension_func_registrar_test",
    srcs = ["string_extension_func_registrar_test.cc"],
    deps = [
        ":builtin_func_registrar",
        ":cel_function_registry",
        ":cel_value",
        ":string_extension_func_registrar",
        "//eval/public/containers:container_backed_list_impl",
        "//internal:testing",
        "@com_google_absl//absl/types:span",
        "@com_google_cel_spec//proto/cel/expr:checked_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)
