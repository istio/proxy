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
    name = "activation_interface",
    hdrs = ["activation_interface.h"],
    deps = [
        ":function_overload_reference",
        "//base:attributes",
        "//common:value",
        "//internal:status_macros",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:optional",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "function_overload_reference",
    hdrs = ["function_overload_reference.h"],
    deps = [
        "//base:function",
        "//base:function_descriptor",
    ],
)

cc_library(
    name = "function_provider",
    hdrs = ["function_provider.h"],
    deps = [
        ":activation_interface",
        ":function_overload_reference",
        "//base:function_descriptor",
        "@com_google_absl//absl/status:statusor",
    ],
)

cc_library(
    name = "activation",
    srcs = ["activation.cc"],
    hdrs = ["activation.h"],
    deps = [
        ":activation_interface",
        ":function_overload_reference",
        "//base:attributes",
        "//base:function",
        "//base:function_descriptor",
        "//common:value",
        "//internal:status_macros",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/types:optional",
        "@com_google_absl//absl/types:span",
    ],
)

cc_test(
    name = "activation_test",
    srcs = ["activation_test.cc"],
    deps = [
        ":activation",
        "//base:attributes",
        "//base:data",
        "//base:function",
        "//base:function_descriptor",
        "//common:memory",
        "//common:value",
        "//internal:testing",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/types:optional",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "register_function_helper",
    hdrs = ["register_function_helper.h"],
    deps =
        [
            ":function_registry",
            "@com_google_absl//absl/status",
            "@com_google_absl//absl/strings",
        ],
)

cc_library(
    name = "function_registry",
    srcs = ["function_registry.cc"],
    hdrs = ["function_registry.h"],
    deps =
        [
            ":activation_interface",
            ":function_overload_reference",
            ":function_provider",
            "//base:function",
            "//base:function_descriptor",
            "//base:kind",
            "@com_google_absl//absl/container:flat_hash_map",
            "@com_google_absl//absl/container:node_hash_map",
            "@com_google_absl//absl/status",
            "@com_google_absl//absl/status:statusor",
            "@com_google_absl//absl/strings",
            "@com_google_absl//absl/types:optional",
            "@com_google_absl//absl/types:span",
        ],
)

cc_test(
    name = "function_registry_test",
    srcs = ["function_registry_test.cc"],
    deps = [
        ":activation",
        ":function_overload_reference",
        ":function_provider",
        ":function_registry",
        "//base:function",
        "//base:function_adapter",
        "//base:function_descriptor",
        "//base:kind",
        "//common:value",
        "//internal:testing",
        "@com_google_absl//absl/status",
    ],
)

cc_library(
    name = "runtime_options",
    hdrs = ["runtime_options.h"],
    deps = ["@com_google_absl//absl/base:core_headers"],
)

cc_library(
    name = "type_registry",
    srcs = ["type_registry.cc"],
    hdrs = ["type_registry.h"],
    deps = [
        "//base:data",
        "//runtime/internal:composed_type_provider",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "runtime",
    hdrs = ["runtime.h"],
    deps = [
        ":activation_interface",
        ":runtime_issue",
        "//base:ast",
        "//base:data",
        "//common:native_type",
        "//common:value",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
    ],
)

cc_library(
    name = "runtime_builder",
    hdrs = ["runtime_builder.h"],
    deps = [
        ":function_registry",
        ":runtime",
        ":runtime_options",
        ":type_registry",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/status:statusor",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "managed_value_factory",
    hdrs = ["managed_value_factory.h"],
    deps = [
        "//base:data",
        "//common:memory",
        "//common:type",
        "//common:value",
    ],
)

cc_library(
    name = "runtime_builder_factory",
    srcs = ["runtime_builder_factory.cc"],
    hdrs = ["runtime_builder_factory.h"],
    deps = [
        ":runtime_builder",
        ":runtime_options",
        "//internal:status_macros",
        "//runtime/internal:runtime_impl",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/status:statusor",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "standard_runtime_builder_factory",
    srcs = ["standard_runtime_builder_factory.cc"],
    hdrs = ["standard_runtime_builder_factory.h"],
    deps = [
        ":runtime_builder",
        ":runtime_builder_factory",
        ":runtime_options",
        ":standard_functions",
        "//internal:status_macros",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/status:statusor",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "standard_runtime_builder_factory_test",
    srcs = ["standard_runtime_builder_factory_test.cc"],
    deps = [
        ":activation",
        ":managed_value_factory",
        ":runtime",
        ":runtime_issue",
        ":runtime_options",
        ":standard_runtime_builder_factory",
        "//common:memory",
        "//common:source",
        "//common:value",
        "//common:value_testing",
        "//extensions:bindings_ext",
        "//extensions/protobuf:memory_manager",
        "//extensions/protobuf:runtime_adapter",
        "//internal:testing",
        "//parser",
        "//parser:macro_registry",
        "//parser:standard_macros",
        "//runtime/internal:runtime_impl",
        "@com_google_absl//absl/base:no_destructor",
        "@com_google_absl//absl/log:absl_check",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "standard_functions",
    srcs = ["standard_functions.cc"],
    hdrs = ["standard_functions.h"],
    deps = [
        ":function_registry",
        ":runtime_options",
        "//internal:status_macros",
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
    name = "constant_folding",
    srcs = ["constant_folding.cc"],
    hdrs = ["constant_folding.h"],
    deps = [
        ":runtime",
        ":runtime_builder",
        "//common:allocator",
        "//common:memory",
        "//common:native_type",
        "//eval/compiler:constant_folding",
        "//internal:casts",
        "//internal:status_macros",
        "//runtime/internal:runtime_friend_access",
        "//runtime/internal:runtime_impl",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/log:absl_check",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "constant_folding_test",
    srcs = ["constant_folding_test.cc"],
    deps = [
        ":activation",
        ":constant_folding",
        ":managed_value_factory",
        ":register_function_helper",
        ":runtime_builder",
        ":runtime_options",
        ":standard_runtime_builder_factory",
        "//base:function_adapter",
        "//common:value",
        "//extensions/protobuf:runtime_adapter",
        "//internal:testing",
        "//internal:testing_descriptor_pool",
        "//parser",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
    ],
)

cc_library(
    name = "regex_precompilation",
    srcs = ["regex_precompilation.cc"],
    hdrs = ["regex_precompilation.h"],
    deps = [
        ":runtime",
        ":runtime_builder",
        "//common:memory",
        "//common:native_type",
        "//eval/compiler:regex_precompilation_optimization",
        "//internal:casts",
        "//internal:status_macros",
        "//runtime/internal:runtime_friend_access",
        "//runtime/internal:runtime_impl",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
    ],
)

cc_test(
    name = "regex_precompilation_test",
    srcs = ["regex_precompilation_test.cc"],
    deps = [
        ":activation",
        ":constant_folding",
        ":managed_value_factory",
        ":regex_precompilation",
        ":register_function_helper",
        ":runtime_builder",
        ":runtime_options",
        ":standard_runtime_builder_factory",
        "//base:function_adapter",
        "//common:value",
        "//extensions/protobuf:runtime_adapter",
        "//internal:testing",
        "//internal:testing_descriptor_pool",
        "//parser",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
    ],
)

cc_library(
    name = "reference_resolver",
    srcs = ["reference_resolver.cc"],
    hdrs = ["reference_resolver.h"],
    deps = [
        ":runtime",
        ":runtime_builder",
        "//common:native_type",
        "//eval/compiler:qualified_reference_resolver",
        "//internal:casts",
        "//internal:status_macros",
        "//runtime/internal:runtime_friend_access",
        "//runtime/internal:runtime_impl",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/log:absl_log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
    ],
)

cc_test(
    name = "reference_resolver_test",
    srcs = ["reference_resolver_test.cc"],
    deps = [
        ":activation",
        ":managed_value_factory",
        ":reference_resolver",
        ":register_function_helper",
        ":runtime_builder",
        ":runtime_options",
        ":standard_runtime_builder_factory",
        "//base:function_adapter",
        "//common:value",
        "//extensions/protobuf:runtime_adapter",
        "//internal:testing",
        "//internal:testing_descriptor_pool",
        "//parser",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_googleapis//google/api/expr/v1alpha1:checked_cc_proto",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "runtime_issue",
    hdrs = ["runtime_issue.h"],
    deps = ["@com_google_absl//absl/status"],
)

cc_library(
    name = "comprehension_vulnerability_check",
    srcs = ["comprehension_vulnerability_check.cc"],
    hdrs = ["comprehension_vulnerability_check.h"],
    deps = [
        ":runtime",
        ":runtime_builder",
        "//common:native_type",
        "//eval/compiler:comprehension_vulnerability_check",
        "//internal:casts",
        "//internal:status_macros",
        "//runtime/internal:runtime_friend_access",
        "//runtime/internal:runtime_impl",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
    ],
)

cc_test(
    name = "comprehension_vulnerability_check_test",
    srcs = ["comprehension_vulnerability_check_test.cc"],
    deps = [
        ":comprehension_vulnerability_check",
        ":runtime_builder",
        ":runtime_options",
        ":standard_runtime_builder_factory",
        "//extensions/protobuf:runtime_adapter",
        "//internal:testing",
        "//internal:testing_descriptor_pool",
        "//parser",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "function_adapter",
    hdrs = ["function_adapter.h"],
    deps = [
        ":register_function_helper",
        "//base:function",
        "//base:function_descriptor",
        "//common:kind",
        "//common:value",
        "//internal:status_macros",
        "//runtime/internal:function_adapter",
        "@com_google_absl//absl/functional:bind_front",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
    ],
)

cc_test(
    name = "function_adapter_test",
    srcs = ["function_adapter_test.cc"],
    deps = [
        ":function_adapter",
        "//base:function",
        "//base:function_descriptor",
        "//common:kind",
        "//common:memory",
        "//common:value",
        "//internal:testing",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
    ],
)

cc_library(
    name = "optional_types",
    srcs = ["optional_types.cc"],
    hdrs = ["optional_types.h"],
    deps = [
        ":function_registry",
        ":runtime_builder",
        ":runtime_options",
        "//base:function_adapter",
        "//common:casting",
        "//common:type",
        "//common:value",
        "//internal:casts",
        "//internal:number",
        "//internal:status_macros",
        "//runtime/internal:errors",
        "//runtime/internal:runtime_friend_access",
        "//runtime/internal:runtime_impl",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/types:optional",
    ],
)

cc_test(
    name = "optional_types_test",
    srcs = ["optional_types_test.cc"],
    deps = [
        ":activation",
        ":optional_types",
        ":reference_resolver",
        ":runtime",
        ":runtime_builder",
        ":runtime_options",
        ":standard_runtime_builder_factory",
        "//base:function",
        "//base:function_descriptor",
        "//common:kind",
        "//common:memory",
        "//common:value",
        "//common:value_testing",
        "//extensions/protobuf:memory_manager",
        "//extensions/protobuf:runtime_adapter",
        "//internal:testing",
        "//internal:testing_descriptor_pool",
        "//parser",
        "//parser:options",
        "//runtime/internal:runtime_impl",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/types:span",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)
