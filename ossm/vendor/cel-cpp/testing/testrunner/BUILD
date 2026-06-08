load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("@rules_cc//cc:cc_test.bzl", "cc_test")

package(
    default_testonly = True,
    default_visibility = ["//visibility:public"],
)

licenses(["notice"])

cc_library(
    name = "cel_test_context",
    hdrs = ["cel_test_context.h"],
    deps = [
        ":cel_expression_source",
        "//common:value",
        "//compiler",
        "//eval/public:cel_expression",
        "//runtime",
        "//runtime:activation",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status:statusor",
        "@com_google_cel_spec//proto/cel/expr:checked_cc_proto",
        "@com_google_cel_spec//proto/cel/expr:value_cc_proto",
        "@com_google_cel_spec//proto/cel/expr/conformance/test:suite_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "runner_lib",
    srcs = ["runner_lib.cc"],
    hdrs = ["runner_lib.h"],
    deps = [
        ":cel_expression_source",
        ":cel_test_context",
        ":coverage_index",
        ":coverage_reporting",
        "//checker:validation_result",
        "//common:ast",
        "//common:ast_proto",
        "//common:value",
        "//common/internal:value_conversion",
        "//eval/public:activation",
        "//eval/public:cel_expression",
        "//eval/public:cel_value",
        "//eval/public:transform_utility",
        "//internal:status_macros",
        "//internal:testing_no_main",
        "//runtime",
        "//runtime:activation",
        "@com_google_absl//absl/functional:overload",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_cel_spec//proto/cel/expr:value_cc_proto",
        "@com_google_cel_spec//proto/cel/expr/conformance/test:suite_cc_proto",
        "@com_google_protobuf//:differencer",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "cel_test_factories",
    hdrs = ["cel_test_factories.h"],
    deps = [
        ":cel_test_context",
        "@com_google_absl//absl/base:no_destructor",
        "@com_google_absl//absl/log:absl_check",
        "@com_google_absl//absl/status:statusor",
        "@com_google_cel_spec//proto/cel/expr/conformance/test:suite_cc_proto",
    ],
)

cc_test(
    name = "runner_lib_test",
    srcs = ["runner_lib_test.cc"],
    args = [
        "--test_cel_file_path=$(location //testing/testrunner/resources:test.cel)",
    ],
    data = [
        "//testing/testrunner/resources:test.cel",
    ],
    deps = [
        ":cel_expression_source",
        ":cel_test_context",
        ":coverage_index",
        ":runner_lib",
        "//checker:type_checker_builder",
        "//checker:validation_result",
        "//common:ast_proto",
        "//common:decl",
        "//common:type",
        "//common:value",
        "//compiler",
        "//compiler:compiler_factory",
        "//compiler:standard_library",
        "//eval/public:builtin_func_registrar",
        "//eval/public:cel_expr_builder_factory",
        "//eval/public:cel_expression",
        "//internal:status_macros",
        "//internal:testing",
        "//internal:testing_descriptor_pool",
        "//runtime",
        "//runtime:activation",
        "//runtime:runtime_builder",
        "//runtime:standard_runtime_builder_factory",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/log:absl_check",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_cel_spec//proto/cel/expr/conformance/proto3:test_all_types_cc_proto",
        "@com_google_cel_spec//proto/cel/expr/conformance/test:suite_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "coverage_reporting",
    srcs = ["coverage_reporting.cc"],
    hdrs = ["coverage_reporting.h"],
    deps = [
        ":coverage_index",
        "//internal:testing_no_main",
        "@com_google_absl//absl/log:absl_log",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
    ],
)

cc_library(
    name = "runner",
    srcs = ["runner_bin.cc"],
    deps = [
        ":cel_expression_source",
        ":cel_test_context",
        ":cel_test_factories",
        ":coverage_index",
        ":coverage_reporting",
        ":runner_lib",
        "//eval/public:cel_expression",
        "//internal:status_macros",
        "//internal:testing_no_main",
        "//runtime",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/log:absl_check",
        "@com_google_absl//absl/log:absl_log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_cel_spec//proto/cel/expr:checked_cc_proto",
        "@com_google_cel_spec//proto/cel/expr/conformance/test:suite_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
    alwayslink = True,
)

cc_library(
    name = "cel_expression_source",
    hdrs = ["cel_expression_source.h"],
    deps = ["@com_google_cel_spec//proto/cel/expr:checked_cc_proto"],
)

cc_library(
    name = "coverage_index",
    srcs = ["coverage_index.cc"],
    hdrs = ["coverage_index.h"],
    deps = [
        "//common:ast",
        "//common:value",
        "//eval/compiler:cel_expression_builder_flat_impl",
        "//eval/compiler:instrumentation",
        "//eval/public:cel_expression",
        "//internal:casts",
        "//runtime",
        "//runtime/internal:runtime_impl",
        "//tools:cel_unparser",
        "//tools:navigable_ast",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_cel_spec//proto/cel/expr:syntax_cc_proto",
    ],
)

cc_test(
    name = "coverage_index_test",
    srcs = ["coverage_index_test.cc"],
    deps = [
        ":coverage_index",
        "//checker:type_checker_builder",
        "//checker:validation_result",
        "//common:ast",
        "//common:ast_proto",
        "//common:decl",
        "//common:type",
        "//common:value",
        "//compiler",
        "//compiler:compiler_factory",
        "//compiler:standard_library",
        "//internal:status_macros",
        "//internal:testing",
        "//internal:testing_descriptor_pool",
        "//runtime",
        "//runtime:activation",
        "//runtime:runtime_builder",
        "//runtime:standard_runtime_builder_factory",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_cel_spec//proto/cel/expr:syntax_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)
