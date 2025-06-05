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

load("//bazel:cel_cc_embed.bzl", "cel_cc_embed")
load("//bazel:cel_proto_transitive_descriptor_set.bzl", "cel_proto_transitive_descriptor_set")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

cc_library(
    name = "align",
    hdrs = ["align.h"],
    deps = [
        "@com_google_absl//absl/base",
        "@com_google_absl//absl/base:config",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/numeric:bits",
    ],
)

cc_test(
    name = "align_test",
    srcs = ["align_test.cc"],
    deps = [
        ":align",
        ":testing",
    ],
)

cc_library(
    name = "new",
    srcs = ["new.cc"],
    hdrs = ["new.h"],
    deps = [
        ":align",
        "@com_google_absl//absl/base:config",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/log:absl_check",
        "@com_google_absl//absl/numeric:bits",
    ],
)

cc_test(
    name = "new_test",
    srcs = ["new_test.cc"],
    deps = [
        ":new",
        ":testing",
    ],
)

cc_library(
    name = "copy_on_write",
    hdrs = ["copy_on_write.h"],
    deps = [
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/log:absl_check",
    ],
)

cc_test(
    name = "copy_on_write_test",
    srcs = ["copy_on_write_test.cc"],
    deps = [
        ":copy_on_write",
        ":testing",
    ],
)

cc_library(
    name = "deserialize",
    srcs = ["deserialize.cc"],
    hdrs = ["deserialize.h"],
    deps = [
        ":proto_wire",
        ":status_macros",
        "//common:any",
        "//common:json",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings:cord",
        "@com_google_absl//absl/time",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "serialize",
    srcs = ["serialize.cc"],
    hdrs = ["serialize.h"],
    deps = [
        ":proto_wire",
        ":status_macros",
        "//common:json",
        "@com_google_absl//absl/base",
        "@com_google_absl//absl/functional:overload",
        "@com_google_absl//absl/log:absl_check",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings:cord",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:variant",
    ],
)

cc_library(
    name = "benchmark",
    testonly = True,
    hdrs = ["benchmark.h"],
    deps = [
        "@com_github_google_benchmark//:benchmark_main",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:cord",
    ],
)

cc_library(
    name = "casts",
    hdrs = ["casts.h"],
)

cc_library(
    name = "status_builder",
    hdrs = ["status_builder.h"],
    deps = [
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/status",
    ],
)

cc_library(
    name = "overflow",
    srcs = ["overflow.cc"],
    hdrs = ["overflow.h"],
    deps = [
        ":status_macros",
        ":time",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/time",
    ],
)

cc_test(
    name = "overflow_test",
    srcs = ["overflow_test.cc"],
    deps = [
        ":overflow",
        ":testing",
        "@com_google_absl//absl/functional:function_ref",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/time",
    ],
)

cc_library(
    name = "number",
    hdrs = ["number.h"],
    deps = ["@com_google_absl//absl/types:variant"],
)

cc_test(
    name = "number_test",
    srcs = ["number_test.cc"],
    deps = [
        ":number",
        ":testing",
        "@com_google_absl//absl/types:optional",
    ],
)

cc_library(
    name = "exceptions",
    hdrs = ["exceptions.h"],
    deps = ["@com_google_absl//absl/base:config"],
)

cc_library(
    name = "status_macros",
    hdrs = ["status_macros.h"],
    deps = [
        ":status_builder",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
    ],
)

cc_library(
    name = "string_pool",
    srcs = ["string_pool.cc"],
    hdrs = ["string_pool.h"],
    deps = [
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/log:die_if_null",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "string_pool_test",
    srcs = ["string_pool_test.cc"],
    deps = [
        ":string_pool",
        ":testing",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "strings",
    srcs = ["strings.cc"],
    hdrs = ["strings.h"],
    deps = [
        ":lexis",
        ":unicode",
        ":utf8",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:cord",
    ],
)

cc_test(
    name = "strings_test",
    srcs = ["strings_test.cc"],
    deps = [
        ":strings",
        ":testing",
        ":utf8",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:cord",
        "@com_google_absl//absl/strings:cord_test_helpers",
        "@com_google_absl//absl/strings:str_format",
    ],
)

cc_library(
    name = "lexis",
    srcs = ["lexis.cc"],
    hdrs = ["lexis.h"],
    deps = [
        "@com_google_absl//absl/base",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "lexis_test",
    srcs = ["lexis_test.cc"],
    deps = [
        ":lexis",
        ":testing",
    ],
)

cc_library(
    name = "proto_util",
    srcs = ["proto_util.cc"],
    hdrs = ["proto_util.h"],
    deps = [
        ":status_macros",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "proto_util_test",
    srcs = ["proto_util_test.cc"],
    deps = [
        ":proto_util",
        ":testing",
        "//eval/public/structs:cel_proto_descriptor_pool_builder",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "proto_time_encoding",
    srcs = ["proto_time_encoding.cc"],
    hdrs = ["proto_time_encoding.h"],
    deps = [
        ":status_macros",
        ":time",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/time",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "proto_time_encoding_test",
    srcs = ["proto_time_encoding_test.cc"],
    deps = [
        ":proto_time_encoding",
        ":testing",
        "//testutil:util",
        "@com_google_absl//absl/time",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "testing",
    testonly = True,
    srcs = [
        "testing.cc",
    ],
    hdrs = [
        "testing.h",
    ],
    deps = [
        ":status_macros",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "testing_no_main",
    testonly = True,
    srcs = [
        "testing.cc",
    ],
    hdrs = [
        "testing.h",
    ],
    deps = [
        ":status_macros",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_googletest//:gtest",
    ],
)

cc_library(
    name = "time",
    srcs = ["time.cc"],
    hdrs = ["time.h"],
    deps = [
        ":status_macros",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/time",
    ],
)

cc_test(
    name = "time_test",
    srcs = ["time_test.cc"],
    deps = [
        ":testing",
        ":time",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/time",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "unicode",
    hdrs = ["unicode.h"],
)

cc_library(
    name = "utf8",
    srcs = ["utf8.cc"],
    hdrs = ["utf8.h"],
    deps = [
        ":unicode",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/log:absl_check",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:cord",
    ],
)

cc_test(
    name = "utf8_test",
    srcs = ["utf8_test.cc"],
    deps = [
        ":benchmark",
        ":testing",
        ":utf8",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:cord",
        "@com_google_absl//absl/strings:cord_test_helpers",
    ],
)

cc_library(
    name = "proto_wire",
    srcs = ["proto_wire.cc"],
    hdrs = ["proto_wire.h"],
    deps = [
        "@com_google_absl//absl/base",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/log:absl_check",
        "@com_google_absl//absl/numeric:bits",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:cord",
        "@com_google_absl//absl/types:optional",
    ],
)

cc_test(
    name = "proto_wire_test",
    srcs = ["proto_wire_test.cc"],
    deps = [
        ":proto_wire",
        ":testing",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:cord",
    ],
)

cc_library(
    name = "proto_matchers",
    testonly = True,
    hdrs = ["proto_matchers.h"],
    deps = [
        ":casts",
        ":testing",
        "@com_google_absl//absl/log:absl_check",
        "@com_google_absl//absl/memory",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "proto_file_util",
    testonly = True,
    hdrs = ["proto_file_util.h"],
    deps = [
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "names",
    srcs = ["names.cc"],
    hdrs = ["names.h"],
    deps = [
        ":lexis",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
    ],
)

cc_test(
    name = "names_test",
    srcs = ["names_test.cc"],
    deps = [
        ":names",
        ":testing",
    ],
)

cc_library(
    name = "to_address",
    hdrs = ["to_address.h"],
    deps = [
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/meta:type_traits",
    ],
)

cc_test(
    name = "to_address_test",
    srcs = ["to_address_test.cc"],
    deps = [
        ":testing",
        ":to_address",
    ],
)

cel_proto_transitive_descriptor_set(
    name = "minimal_descriptor_set",
    deps = [
        "@com_google_protobuf//:any_proto",
        "@com_google_protobuf//:duration_proto",
        "@com_google_protobuf//:struct_proto",
        "@com_google_protobuf//:timestamp_proto",
        "@com_google_protobuf//:wrappers_proto",
    ],
)

cel_cc_embed(
    name = "minimal_descriptor_set_embed",
    src = ":minimal_descriptor_set",
)

cc_library(
    name = "minimal_descriptor_pool",
    srcs = ["minimal_descriptor_pool.cc"],
    hdrs = ["minimal_descriptor_pool.h"],
    textual_hdrs = [":minimal_descriptor_set_embed"],
    deps = [
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/log:absl_check",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "minimal_descriptor_pool_test",
    srcs = ["minimal_descriptor_pool_test.cc"],
    deps = [
        ":minimal_descriptor_pool",
        ":testing",
        "@com_google_protobuf//:protobuf",
    ],
)

cel_proto_transitive_descriptor_set(
    name = "testing_descriptor_set",
    testonly = True,
    deps = [
        "@com_google_cel_spec//proto/cel/expr:expr_proto",
        "@com_google_cel_spec//proto/test/v1/proto2:test_all_types_proto",
        "@com_google_cel_spec//proto/test/v1/proto3:test_all_types_proto",
        "@com_google_googleapis//google/api/expr/v1alpha1:checked_proto",
        "@com_google_googleapis//google/api/expr/v1alpha1:eval_proto",
        "@com_google_googleapis//google/api/expr/v1alpha1:explain_proto",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_proto",
        "@com_google_googleapis//google/api/expr/v1alpha1:value_proto",
        "@com_google_protobuf//:any_proto",
        "@com_google_protobuf//:duration_proto",
        "@com_google_protobuf//:empty_proto",
        "@com_google_protobuf//:field_mask_proto",
        "@com_google_protobuf//:struct_proto",
        "@com_google_protobuf//:timestamp_proto",
        "@com_google_protobuf//:wrappers_proto",
    ],
)

cel_cc_embed(
    name = "testing_descriptor_set_embed",
    testonly = True,
    src = ":testing_descriptor_set",
)

cc_library(
    name = "testing_descriptor_pool",
    testonly = True,
    srcs = ["testing_descriptor_pool.cc"],
    hdrs = ["testing_descriptor_pool.h"],
    textual_hdrs = [":testing_descriptor_set_embed"],
    deps = [
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/base:no_destructor",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/log:absl_check",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "testing_descriptor_pool_test",
    srcs = ["testing_descriptor_pool_test.cc"],
    deps = [
        ":testing",
        ":testing_descriptor_pool",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "message_type_name",
    hdrs = ["message_type_name.h"],
    deps = [
        "@com_google_absl//absl/base:no_destructor",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "message_type_name_test",
    srcs = ["message_type_name_test.cc"],
    deps = [
        ":message_type_name",
        ":testing",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "parse_text_proto",
    testonly = True,
    hdrs = ["parse_text_proto.h"],
    deps = [
        ":message_type_name",
        ":testing_descriptor_pool",
        ":testing_message_factory",
        "//common:allocator",
        "//common:memory",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/log:absl_check",
        "@com_google_absl//absl/log:die_if_null",
        "@com_google_absl//absl/strings:cord",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "equals_text_proto",
    testonly = True,
    srcs = ["equals_text_proto.cc"],
    hdrs = ["equals_text_proto.h"],
    deps = [
        ":parse_text_proto",
        ":testing",
        ":testing_descriptor_pool",
        ":testing_message_factory",
        "//common:allocator",
        "//common:memory",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/log:absl_check",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings:cord",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "testing_message_factory",
    testonly = True,
    srcs = ["testing_message_factory.cc"],
    hdrs = ["testing_message_factory.h"],
    deps = [
        ":testing_descriptor_pool",
        "@com_google_absl//absl/base:no_destructor",
        "@com_google_absl//absl/base:nullability",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "well_known_types",
    srcs = ["well_known_types.cc"],
    hdrs = ["well_known_types.h"],
    deps = [
        ":protobuf_runtime_version",
        ":status_macros",
        "//common:any",
        "//common:json",
        "//common:memory",
        "//extensions/protobuf/internal:map_reflection",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/base:no_destructor",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/functional:overload",
        "@com_google_absl//absl/log:absl_check",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:cord",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:variant",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "well_known_types_test",
    srcs = ["well_known_types_test.cc"],
    deps = [
        ":message_type_name",
        ":minimal_descriptor_pool",
        ":parse_text_proto",
        ":testing",
        ":testing_descriptor_pool",
        ":testing_message_factory",
        ":well_known_types",
        "//common:memory",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/log:die_if_null",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings:cord",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:variant",
        "@com_google_cel_spec//proto/test/v1/proto3:test_all_types_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "json",
    srcs = ["json.cc"],
    hdrs = ["json.h"],
    deps = [
        ":status_macros",
        ":strings",
        ":well_known_types",
        "//common:json",
        "//extensions/protobuf/internal:map_reflection",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/base:no_destructor",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/functional:overload",
        "@com_google_absl//absl/log:absl_check",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:cord",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/types:variant",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "json_test",
    srcs = ["json_test.cc"],
    deps = [
        ":equals_text_proto",
        ":json",
        ":message_type_name",
        ":parse_text_proto",
        ":proto_matchers",
        ":testing",
        ":testing_descriptor_pool",
        ":testing_message_factory",
        "//common:json",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/log:die_if_null",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_cel_spec//proto/test/v1/proto3:test_all_types_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "message_equality",
    srcs = ["message_equality.cc"],
    hdrs = ["message_equality.h"],
    deps = [
        ":json",
        ":number",
        ":status_macros",
        ":well_known_types",
        "//common:memory",
        "//extensions/protobuf/internal:map_reflection",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/functional:overload",
        "@com_google_absl//absl/log:absl_check",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:variant",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "message_equality_test",
    srcs = ["message_equality_test.cc"],
    deps = [
        ":message_equality",
        ":message_type_name",
        ":parse_text_proto",
        ":testing",
        ":testing_descriptor_pool",
        ":testing_message_factory",
        ":well_known_types",
        "//common:allocator",
        "//common:memory",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/log:absl_check",
        "@com_google_absl//absl/log:die_if_null",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:cord",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/types:optional",
        "@com_google_cel_spec//proto/test/v1/proto3:test_all_types_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "protobuf_runtime_version",
    hdrs = ["protobuf_runtime_version.h"],
    deps = ["@com_google_protobuf//:protobuf"],
)
