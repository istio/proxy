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

licenses(["notice"])

cc_library(
    name = "ast",
    srcs = ["ast.cc"],
    hdrs = ["ast.h"],
    deps = [
        ":expr",
        "//common/ast:metadata",
        "@com_google_absl//absl/base:no_destructor",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/strings:string_view",
    ],
)

cc_test(
    name = "ast_test",
    srcs = ["ast_test.cc"],
    deps = [
        ":ast",
        ":expr",
        "//internal:testing",
        "@com_google_absl//absl/container:flat_hash_map",
    ],
)

cc_library(
    name = "expr",
    srcs = ["expr.cc"],
    hdrs = ["expr.h"],
    deps = [
        ":constant",
        "@com_google_absl//absl/algorithm:container",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/base:no_destructor",
        "@com_google_absl//absl/functional:overload",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/types:optional",
        "@com_google_absl//absl/types:span",
        "@com_google_absl//absl/types:variant",
    ],
)

cc_test(
    name = "expr_test",
    srcs = ["expr_test.cc"],
    deps = [
        ":expr",
        "//internal:testing",
    ],
)

cc_library(
    name = "navigable_ast",
    srcs = ["navigable_ast.cc"],
    hdrs = ["navigable_ast.h"],
    deps = [
        ":ast_traverse",
        ":ast_visitor",
        ":ast_visitor_base",
        ":expr",
        "//common/ast:navigable_ast_internal",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/types:optional",
    ],
)

cc_test(
    name = "navigable_ast_test",
    srcs = ["navigable_ast_test.cc"],
    deps = [
        ":ast",
        ":expr",
        ":navigable_ast",
        ":source",
        ":standard_definitions",
        "//internal:status_macros",
        "//internal:testing",
        "//parser",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings:string_view",
    ],
)

cc_library(
    name = "decl",
    srcs = ["decl.cc"],
    hdrs = ["decl.h"],
    deps = [
        ":constant",
        ":type",
        ":type_kind",
        "//internal:status_macros",
        "@com_google_absl//absl/algorithm:container",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/hash",
        "@com_google_absl//absl/log:absl_check",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/types:optional",
        "@com_google_absl//absl/types:span",
    ],
)

cc_test(
    name = "decl_test",
    srcs = ["decl_test.cc"],
    deps = [
        ":constant",
        ":decl",
        ":type",
        "//internal:testing",
        "@com_google_absl//absl/status",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "reference",
    srcs = ["reference.cc"],
    hdrs = ["reference.h"],
    deps = [
        ":constant",
        "@com_google_absl//absl/algorithm:container",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/base:no_destructor",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/types:variant",
    ],
)

cc_test(
    name = "reference_test",
    srcs = ["reference_test.cc"],
    deps = [
        ":constant",
        ":reference",
        "//internal:testing",
    ],
)

cc_library(
    name = "ast_rewrite",
    srcs = ["ast_rewrite.cc"],
    hdrs = ["ast_rewrite.h"],
    deps = [
        ":ast_visitor",
        ":constant",
        ":expr",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/log:absl_log",
        "@com_google_absl//absl/types:span",
        "@com_google_absl//absl/types:variant",
    ],
)

cc_test(
    name = "ast_rewrite_test",
    srcs = ["ast_rewrite_test.cc"],
    deps = [
        ":ast",
        ":ast_rewrite",
        ":ast_visitor",
        ":expr",
        "//common/ast:expr_proto",
        "//extensions/protobuf:ast_converters",
        "//internal:testing",
        "//parser",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_cel_spec//proto/cel/expr:syntax_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "ast_traverse",
    srcs = ["ast_traverse.cc"],
    hdrs = ["ast_traverse.h"],
    deps = [
        ":ast_visitor",
        ":constant",
        ":expr",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/log:absl_log",
        "@com_google_absl//absl/types:variant",
    ],
)

cc_test(
    name = "ast_traverse_test",
    srcs = ["ast_traverse_test.cc"],
    deps = [
        ":ast_traverse",
        ":ast_visitor",
        ":constant",
        ":expr",
        "//internal:testing",
    ],
)

cc_library(
    name = "ast_visitor",
    hdrs = ["ast_visitor.h"],
    deps = [
        ":constant",
        ":expr",
    ],
)

cc_library(
    name = "ast_visitor_base",
    hdrs = ["ast_visitor_base.h"],
    deps = [
        ":ast_visitor",
        ":constant",
        ":expr",
    ],
)

cc_library(
    name = "constant",
    srcs = ["constant.cc"],
    hdrs = ["constant.h"],
    deps = [
        "//internal:strings",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/base:no_destructor",
        "@com_google_absl//absl/functional:overload",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:variant",
    ],
)

cc_test(
    name = "constant_test",
    srcs = ["constant_test.cc"],
    deps = [
        ":constant",
        "//internal:testing",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/time",
    ],
)

cc_library(
    name = "expr_factory",
    hdrs = ["expr_factory.h"],
    deps = [
        ":constant",
        ":expr",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "operators",
    srcs = [
        "operators.cc",
    ],
    hdrs = [
        "operators.h",
    ],
    deps = [
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:optional",
        "@com_google_cel_spec//proto/cel/expr:syntax_cc_proto",
    ],
)

cc_library(
    name = "any",
    srcs = ["any.cc"],
    hdrs = ["any.h"],
    deps = [
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:cord",
        "@com_google_protobuf//:any_cc_proto",
    ],
)

cc_test(
    name = "any_test",
    srcs = ["any_test.cc"],
    deps = [
        ":any",
        "//internal:testing",
        "@com_google_absl//absl/strings:cord",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_protobuf//:any_cc_proto",
    ],
)

cc_library(
    name = "casting",
    hdrs = ["casting.h"],
    deps = [
        "//common/internal:casting",
        "@com_google_absl//absl/base:core_headers",
    ],
)

cc_library(
    name = "json",
    hdrs = ["json.h"],
)

cc_library(
    name = "kind",
    srcs = ["kind.cc"],
    hdrs = ["kind.h"],
    deps = [
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "kind_test",
    srcs = ["kind_test.cc"],
    deps = [
        ":kind",
        ":type_kind",
        ":value_kind",
        "//internal:testing",
    ],
)

cc_library(
    name = "memory",
    srcs = ["memory.cc"],
    hdrs = ["memory.h"],
    deps = [
        ":allocator",
        ":arena",
        ":data",
        ":native_type",
        ":reference_count",
        "//common/internal:metadata",
        "//common/internal:reference_count",
        "//internal:exceptions",
        "//internal:to_address",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/base:no_destructor",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/log:absl_check",
        "@com_google_absl//absl/numeric:bits",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "memory_test",
    srcs = ["memory_test.cc"],
    deps = [
        ":allocator",
        ":data",
        ":memory",
        ":native_type",
        "//common/internal:reference_count",
        "//internal:testing",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/debugging:leak_check",
        "@com_google_absl//absl/log:absl_check",
        "@com_google_absl//absl/types:optional",
        "@com_google_protobuf//:protobuf",
        "@com_google_protobuf//:struct_cc_proto",
    ],
)

cc_library(
    name = "memory_testing",
    testonly = True,
    hdrs = ["memory_testing.h"],
    deps = [
        ":memory",
        "//internal:testing",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:optional",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "type_testing",
    testonly = True,
    hdrs = ["type_testing.h"],
)

cc_library(
    name = "value_testing",
    testonly = True,
    srcs = ["value_testing.cc"],
    hdrs = ["value_testing.h"],
    deps = [
        ":value",
        ":value_kind",
        "//internal:equals_text_proto",
        "//internal:parse_text_proto",
        "//internal:testing_descriptor_pool",
        "//internal:testing_message_factory",
        "//internal:testing_no_main",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/log:die_if_null",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/time",
        "@com_google_protobuf//:protobuf",
        "@com_google_protobuf//:struct_cc_proto",
    ],
)

cc_test(
    name = "value_testing_test",
    srcs = ["value_testing_test.cc"],
    deps = [
        ":value",
        ":value_testing",
        "//internal:testing",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/time",
    ],
)

cc_library(
    name = "type_kind",
    hdrs = ["type_kind.h"],
    deps = [
        ":kind",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "value_kind",
    hdrs = ["value_kind.h"],
    deps = [
        ":kind",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "source",
    srcs = ["source.cc"],
    hdrs = ["source.h"],
    deps = [
        "//internal:unicode",
        "//internal:utf8",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/container:inlined_vector",
        "@com_google_absl//absl/functional:overload",
        "@com_google_absl//absl/log:absl_check",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:cord",
        "@com_google_absl//absl/types:optional",
        "@com_google_absl//absl/types:span",
        "@com_google_absl//absl/types:variant",
    ],
)

cc_test(
    name = "source_test",
    srcs = ["source_test.cc"],
    deps = [
        ":source",
        "//internal:testing",
        "@com_google_absl//absl/strings:cord",
        "@com_google_absl//absl/types:optional",
    ],
)

cc_library(
    name = "native_type",
    hdrs = ["native_type.h"],
    deps = [
        ":typeinfo",
    ],
)

cc_library(
    name = "type",
    srcs = glob(
        [
            "types/*.cc",
        ],
        exclude = [
            "types/*_test.cc",
        ],
    ) + [
        "type.cc",
        "type_introspector.cc",
    ],
    hdrs = glob(
        [
            "types/*.h",
        ],
        exclude = [
            "types/*_test.h",
        ],
    ) + [
        "type.h",
        "type_factory.h",
        "type_introspector.h",
        "type_manager.h",
    ],
    deps = [
        ":memory",
        ":type_kind",
        "//internal:string_pool",
        "@com_google_absl//absl/algorithm:container",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/container:inlined_vector",
        "@com_google_absl//absl/functional:overload",
        "@com_google_absl//absl/hash",
        "@com_google_absl//absl/log:absl_check",
        "@com_google_absl//absl/log:die_if_null",
        "@com_google_absl//absl/meta:type_traits",
        "@com_google_absl//absl/numeric:bits",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/types:optional",
        "@com_google_absl//absl/types:span",
        "@com_google_absl//absl/types:variant",
        "@com_google_absl//absl/utility",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "type_test",
    srcs = glob([
        "types/*_test.cc",
    ]) + [
        "type_test.cc",
    ],
    deps = [
        ":memory",
        ":type",
        ":type_kind",
        "//internal:testing",
        "//internal:testing_descriptor_pool",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/hash",
        "@com_google_absl//absl/hash:hash_testing",
        "@com_google_absl//absl/log:absl_check",
        "@com_google_absl//absl/log:die_if_null",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "value",
    srcs = glob(
        [
            "values/*.cc",
        ],
        exclude = [
            "values/*_test.cc",
        ],
    ) + [
        "legacy_value.cc",
        "value.cc",
    ],
    hdrs = glob(
        [
            "values/*.h",
        ],
        exclude = [
            "values/*_test.h",
        ],
    ) + [
        "legacy_value.h",
        "type_reflector.h",
        "value.h",
    ],
    deps = [
        ":allocator",
        ":any",
        ":arena",
        ":casting",
        ":kind",
        ":memory",
        ":native_type",
        ":optional_ref",
        ":type",
        ":typeinfo",
        ":unknown",
        ":value_kind",
        "//base:attributes",
        "//common/internal:byte_string",
        "//common/internal:reference_count",
        "//eval/internal:cel_value_equal",
        "//eval/public:cel_value",
        "//eval/public:message_wrapper",
        "//eval/public/containers:field_backed_list_impl",
        "//eval/public/containers:field_backed_map_impl",
        "//eval/public/structs:cel_proto_wrap_util",
        "//eval/public/structs:legacy_type_adapter",
        "//eval/public/structs:legacy_type_info_apis",
        "//eval/public/structs:proto_message_type_adapter",
        "//extensions/protobuf/internal:map_reflection",
        "//extensions/protobuf/internal:qualify",
        "//internal:casts",
        "//internal:empty_descriptors",
        "//internal:json",
        "//internal:manual",
        "//internal:message_equality",
        "//internal:number",
        "//internal:protobuf_runtime_version",
        "//internal:status_macros",
        "//internal:strings",
        "//internal:time",
        "//internal:utf8",
        "//internal:well_known_types",
        "//runtime:runtime_options",
        "//runtime/internal:errors",
        "@com_google_absl//absl/base",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/base:no_destructor",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/functional:function_ref",
        "@com_google_absl//absl/functional:overload",
        "@com_google_absl//absl/hash",
        "@com_google_absl//absl/log:absl_check",
        "@com_google_absl//absl/log:absl_log",
        "@com_google_absl//absl/meta:type_traits",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:cord",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:optional",
        "@com_google_absl//absl/types:span",
        "@com_google_absl//absl/types:variant",
        "@com_google_absl//absl/utility",
        "@com_google_protobuf//:any_cc_proto",
        "@com_google_protobuf//:duration_cc_proto",
        "@com_google_protobuf//:empty_cc_proto",
        "@com_google_protobuf//:protobuf",
        "@com_google_protobuf//:struct_cc_proto",
        "@com_google_protobuf//:timestamp_cc_proto",
        "@com_google_protobuf//:wrappers_cc_proto",
        "@com_google_protobuf//src/google/protobuf/io",
    ],
)

cc_test(
    name = "value_test",
    srcs = glob([
        "values/*_test.cc",
    ]) + [
        "type_reflector_test.cc",
        "value_test.cc",
    ],
    deps = [
        ":casting",
        ":memory",
        ":native_type",
        ":type",
        ":value",
        ":value_kind",
        ":value_testing",
        "//base:attributes",
        "//internal:parse_text_proto",
        "//internal:status_macros",
        "//internal:testing",
        "//internal:testing_descriptor_pool",
        "//internal:testing_message_factory",
        "//runtime:runtime_options",
        "//runtime/internal:errors",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/functional:function_ref",
        "@com_google_absl//absl/hash",
        "@com_google_absl//absl/log:die_if_null",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:cord",
        "@com_google_absl//absl/strings:cord_test_helpers",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:optional",
        "@com_google_cel_spec//proto/cel/expr/conformance/proto3:test_all_types_cc_proto",
        "@com_google_protobuf//:protobuf",
        "@com_google_protobuf//:struct_cc_proto",
        "@com_google_protobuf//:type_cc_proto",
        "@com_google_protobuf//src/google/protobuf/io",
    ],
)

cc_library(
    name = "unknown",
    hdrs = ["unknown.h"],
    deps = ["//base/internal:unknown_set"],
)

alias(
    name = "legacy_value",
    actual = ":value",
)

cc_library(
    name = "arena",
    hdrs = ["arena.h"],
    deps = [
        "@com_google_absl//absl/base:nullability",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "reference_count",
    hdrs = ["reference_count.h"],
    deps = ["//common/internal:reference_count"],
)

cc_library(
    name = "allocator",
    hdrs = ["allocator.h"],
    deps = [
        ":arena",
        ":data",
        "//internal:new",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/log:absl_check",
        "@com_google_absl//absl/log:die_if_null",
        "@com_google_absl//absl/numeric:bits",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "allocator_test",
    srcs = ["allocator_test.cc"],
    deps = [
        ":allocator",
        "//internal:testing",
        "@com_google_absl//absl/strings",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "data",
    hdrs = ["data.h"],
    deps = [
        "//common/internal:metadata",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/log:absl_check",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "data_test",
    srcs = ["data_test.cc"],
    deps = [
        ":data",
        "//common/internal:reference_count",
        "//internal:testing",
        "@com_google_absl//absl/base:nullability",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "optional_ref",
    hdrs = ["optional_ref.h"],
    deps = [
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/types:optional",
        "@com_google_absl//absl/utility",
    ],
)

cc_library(
    name = "arena_string",
    hdrs = [
        "arena_string.h",
        "arena_string_view.h",
    ],
    deps = [
        "@com_google_absl//absl/base",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/log:absl_check",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "arena_string_test",
    srcs = [
        "arena_string_test.cc",
        "arena_string_view_test.cc",
    ],
    deps = [
        ":arena_string",
        "//internal:testing",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/hash",
        "@com_google_absl//absl/hash:hash_testing",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "arena_string_pool",
    hdrs = ["arena_string_pool.h"],
    deps = [
        ":arena_string",
        "//internal:string_pool",
        "@com_google_absl//absl/base",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/strings:cord",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "arena_string_pool_test",
    srcs = ["arena_string_pool_test.cc"],
    deps = [
        ":arena_string_pool",
        "//internal:testing",
        "@com_google_absl//absl/strings:cord_test_helpers",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "minimal_descriptor_pool",
    srcs = ["minimal_descriptor_pool.cc"],
    hdrs = ["minimal_descriptor_pool.h"],
    deps = [
        "//internal:minimal_descriptors",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/status",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "minimal_descriptor_pool_test",
    srcs = ["minimal_descriptor_pool_test.cc"],
    deps = [
        ":minimal_descriptor_pool",
        "//internal:testing",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "minimal_descriptor_database",
    srcs = ["minimal_descriptor_database.cc"],
    hdrs = ["minimal_descriptor_database.h"],
    deps = [
        "//internal:minimal_descriptors",
        "@com_google_absl//absl/base:nullability",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "minimal_descriptor_database_test",
    srcs = ["minimal_descriptor_database_test.cc"],
    deps = [
        ":minimal_descriptor_database",
        "//internal:testing",
        "@com_google_protobuf//:protobuf",
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
    name = "decl_proto",
    srcs = ["decl_proto.cc"],
    hdrs = ["decl_proto.h"],
    deps = [
        ":decl",
        ":type",
        ":type_proto",
        "//internal:status_macros",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/types:variant",
        "@com_google_cel_spec//proto/cel/expr:checked_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "decl_proto_test",
    srcs = ["decl_proto_test.cc"],
    deps = [
        ":decl",
        ":decl_proto",
        ":decl_proto_v1alpha1",
        "//internal:testing",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/types:variant",
        "@com_google_googleapis//google/api/expr/v1alpha1:checked_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "decl_proto_v1alpha1",
    srcs = ["decl_proto_v1alpha1.cc"],
    hdrs = ["decl_proto_v1alpha1.h"],
    deps = [
        ":decl",
        ":decl_proto",
        ":type",
        ":type_proto",
        "//internal:status_macros",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/types:variant",
        "@com_google_cel_spec//proto/cel/expr:checked_cc_proto",
        "@com_google_googleapis//google/api/expr/v1alpha1:checked_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "type_proto",
    srcs = ["type_proto.cc"],
    hdrs = ["type_proto.h"],
    deps = [
        ":type",
        ":type_kind",
        "//internal:status_macros",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:optional",
        "@com_google_cel_spec//proto/cel/expr:checked_cc_proto",
        "@com_google_protobuf//:protobuf",
        "@com_google_protobuf//:struct_cc_proto",
    ],
)

cc_test(
    name = "type_proto_test",
    srcs = ["type_proto_test.cc"],
    deps = [
        ":type",
        ":type_kind",
        ":type_proto",
        "//internal:proto_matchers",
        "//internal:testing",
        "//internal:testing_descriptor_pool",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "ast_proto",
    srcs = ["ast_proto.cc"],
    hdrs = ["ast_proto.h"],
    deps = [
        ":ast",
        ":constant",
        ":expr",
        "//base:ast",
        "//common/ast:constant_proto",
        "//common/ast:expr_proto",
        "//common/ast:source_info_proto",
        "//internal:status_macros",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/types:variant",
        "@com_google_cel_spec//proto/cel/expr:checked_cc_proto",
        "@com_google_cel_spec//proto/cel/expr:syntax_cc_proto",
        "@com_google_protobuf//:duration_cc_proto",
        "@com_google_protobuf//:struct_cc_proto",
        "@com_google_protobuf//:timestamp_cc_proto",
    ],
)

cc_test(
    name = "ast_proto_test",
    srcs = [
        "ast_proto_test.cc",
    ],
    deps = [
        ":ast",
        ":ast_proto",
        ":decl",
        ":expr",
        ":source",
        ":type",
        "//compiler",
        "//compiler:compiler_factory",
        "//compiler:optional",
        "//compiler:standard_library",
        "//extensions:comprehensions_v2",
        "//internal:proto_matchers",
        "//internal:status_macros",
        "//internal:testing",
        "//internal:testing_descriptor_pool",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/types:variant",
        "@com_google_cel_spec//proto/cel/expr:checked_cc_proto",
        "@com_google_cel_spec//proto/cel/expr:syntax_cc_proto",
        "@com_google_protobuf//:duration_cc_proto",
        "@com_google_protobuf//:protobuf",
        "@com_google_protobuf//:struct_cc_proto",
        "@com_google_protobuf//:timestamp_cc_proto",
    ],
)

cc_library(
    name = "standard_definitions",
    hdrs = [
        "standard_definitions.h",
    ],
    deps = [
        "@com_google_absl//absl/strings:string_view",
    ],
)

cc_library(
    name = "typeinfo",
    srcs = ["typeinfo.cc"],
    hdrs = ["typeinfo.h"],
    deps = [
        "@com_google_absl//absl/base",
        "@com_google_absl//absl/base:config",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/meta:type_traits",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "typeinfo_test",
    srcs = ["typeinfo_test.cc"],
    deps = [
        ":typeinfo",
        "//internal:testing",
        "@com_google_absl//absl/hash:hash_testing",
        "@com_google_absl//absl/strings",
    ],
)
