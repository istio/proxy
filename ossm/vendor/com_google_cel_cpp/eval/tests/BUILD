# This package contains CEL evaluator tests (end-to-end, benchmark etc.)
#
#

load("@com_google_protobuf//bazel:cc_proto_library.bzl", "cc_proto_library")
load("@com_google_protobuf//bazel:proto_library.bzl", "proto_library")
load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("@rules_cc//cc:cc_test.bzl", "cc_test")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

exports_files(["LICENSE"])

cc_test(
    name = "benchmark_test",
    srcs = [
        "benchmark_test.cc",
    ],
    tags = ["benchmark"],
    deps = [
        ":request_context_cc_proto",
        "//eval/public:activation",
        "//eval/public:builtin_func_registrar",
        "//eval/public:cel_expr_builder_factory",
        "//eval/public:cel_expression",
        "//eval/public:cel_options",
        "//eval/public:cel_value",
        "//eval/public/containers:container_backed_list_impl",
        "//eval/public/containers:container_backed_map_impl",
        "//eval/public/structs:cel_proto_wrapper",
        "//internal:benchmark",
        "//internal:status_macros",
        "//internal:testing",
        "//parser",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:btree",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/container:node_hash_set",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/strings",
        "@com_google_cel_spec//proto/cel/expr:syntax_cc_proto",
        "@com_google_googleapis//google/rpc/context:attribute_context_cc_proto",
        "@com_google_protobuf//:protobuf",
        "@com_google_protobuf//:struct_cc_proto",
    ],
)

cc_test(
    name = "modern_benchmark_test",
    srcs = [
        "modern_benchmark_test.cc",
    ],
    tags = ["benchmark"],
    deps = [
        ":request_context_cc_proto",
        "//common:allocator",
        "//common:casting",
        "//common:legacy_value",
        "//common:memory",
        "//common:native_type",
        "//common:value",
        "//extensions/protobuf:runtime_adapter",
        "//extensions/protobuf:value",
        "//internal:benchmark",
        "//internal:testing",
        "//internal:testing_descriptor_pool",
        "//internal:testing_message_factory",
        "//parser",
        "//runtime",
        "//runtime:activation",
        "//runtime:constant_folding",
        "//runtime:runtime_options",
        "//runtime:standard_runtime_builder_factory",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/container:btree",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/container:node_hash_set",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/log:absl_check",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:optional",
        "@com_google_cel_spec//proto/cel/expr:syntax_cc_proto",
        "@com_google_googleapis//google/rpc/context:attribute_context_cc_proto",
        "@com_google_protobuf//:protobuf",
        "@com_google_protobuf//:struct_cc_proto",
    ],
)

cc_test(
    name = "allocation_benchmark_test",
    size = "small",
    srcs = [
        "allocation_benchmark_test.cc",
    ],
    tags = ["benchmark"],
    deps = [
        ":request_context_cc_proto",
        "//eval/public:activation",
        "//eval/public:builtin_func_registrar",
        "//eval/public:cel_expr_builder_factory",
        "//eval/public:cel_expression",
        "//eval/public:cel_value",
        "//internal:benchmark",
        "//internal:testing",
        "//parser",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_cel_spec//proto/cel/expr:syntax_cc_proto",
        "@com_google_googleapis//google/rpc/context:attribute_context_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "memory_safety_test",
    srcs = [
        "memory_safety_test.cc",
    ],
    deps = [
        "//eval/public:activation",
        "//eval/public:builtin_func_registrar",
        "//eval/public:cel_expr_builder_factory",
        "//eval/public:cel_expression",
        "//eval/public:cel_function_adapter",
        "//eval/public:cel_options",
        "//eval/public/testing:matchers",
        "//internal:testing",
        "//parser",
        "//testutil:util",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_cel_spec//proto/cel/expr:syntax_cc_proto",
        "@com_google_googleapis//google/rpc/context:attribute_context_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "expression_builder_benchmark_test",
    size = "small",
    srcs = [
        "expression_builder_benchmark_test.cc",
    ],
    tags = ["benchmark"],
    deps = [
        ":request_context_cc_proto",
        "//common:minimal_descriptor_pool",
        "//eval/public:builtin_func_registrar",
        "//eval/public:cel_expr_builder_factory",
        "//eval/public:cel_expression",
        "//eval/public:cel_options",
        "//eval/public:cel_type_registry",
        "//internal:benchmark",
        "//internal:status_macros",
        "//internal:testing",
        "//parser",
        "@com_google_absl//absl/log:absl_check",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_cel_spec//proto/cel/expr:checked_cc_proto",
        "@com_google_cel_spec//proto/cel/expr:syntax_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "end_to_end_test",
    size = "small",
    srcs = [
        "end_to_end_test.cc",
    ],
    deps = [
        "//eval/public:activation",
        "//eval/public:builtin_func_registrar",
        "//eval/public:cel_expr_builder_factory",
        "//eval/public:cel_expression",
        "//eval/public:cel_value",
        "//eval/public/structs:cel_proto_wrapper",
        "//eval/testutil:test_message_cc_proto",
        "//internal:status_macros",
        "//internal:testing",
        "//testutil:util",
        "@com_google_absl//absl/status",
        "@com_google_cel_spec//proto/cel/expr:syntax_cc_proto",
        "@com_google_protobuf//:protobuf",
        "@com_google_protobuf//:struct_cc_proto",
    ],
)

cc_test(
    name = "unknowns_end_to_end_test",
    size = "small",
    srcs = [
        "unknowns_end_to_end_test.cc",
    ],
    deps = [
        "//base:attributes",
        "//base:function_result",
        "//eval/public:activation",
        "//eval/public:builtin_func_registrar",
        "//eval/public:cel_attribute",
        "//eval/public:cel_expr_builder_factory",
        "//eval/public:cel_expression",
        "//eval/public:cel_function",
        "//eval/public:cel_options",
        "//eval/public:cel_value",
        "//eval/public:unknown_set",
        "//eval/public/containers:container_backed_map_impl",
        "//eval/public/structs:cel_proto_wrapper",
        "//internal:status_macros",
        "//internal:testing",
        "//parser",
        "//runtime/internal:activation_attribute_matcher_access",
        "//runtime/internal:attribute_matcher",
        "@com_google_absl//absl/base:no_destructor",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
        "@com_google_cel_spec//proto/cel/expr:syntax_cc_proto",
        "@com_google_protobuf//:protobuf",
        "@com_google_protobuf//:struct_cc_proto",
    ],
)

proto_library(
    name = "request_context_protos",
    srcs = [
        "request_context.proto",
    ],
)

cc_proto_library(
    name = "request_context_cc_proto",
    deps = [":request_context_protos"],
)

cc_library(
    name = "mock_cel_expression",
    testonly = 1,
    hdrs = ["mock_cel_expression.h"],
    deps = [
        "//eval/public:base_activation",
        "//eval/public:cel_expression",
        "//internal:testing_no_main",
        "@com_google_absl//absl/status:statusor",
    ],
)
