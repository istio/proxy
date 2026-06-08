load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("//testing/testrunner:cel_cc_test.bzl", "cel_cc_test")

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "simple_user_test",
    testonly = True,
    srcs = ["simple.cc"],
    deps = [
        "//checker:type_checker_builder",
        "//checker:validation_result",
        "//common:ast_proto",
        "//common:decl",
        "//common:type",
        "//compiler",
        "//compiler:compiler_factory",
        "//compiler:standard_library",
        "//internal:status_macros",
        "//internal:testing_descriptor_pool",
        "//runtime",
        "//runtime:runtime_builder",
        "//runtime:standard_runtime_builder_factory",
        "//testing/testrunner:cel_expression_source",
        "//testing/testrunner:cel_test_context",
        "//testing/testrunner:cel_test_factories",
        "@com_google_absl//absl/log:absl_check",
        "@com_google_absl//absl/log:absl_log",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_cel_spec//proto/cel/expr:checked_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
    alwayslink = True,
)

cc_library(
    name = "raw_expression_user_test",
    testonly = True,
    srcs = ["raw_expression_test.cc"],
    deps = [
        "//checker:type_checker_builder",
        "//common:decl",
        "//common:type",
        "//compiler",
        "//compiler:compiler_factory",
        "//compiler:standard_library",
        "//internal:status_macros",
        "//internal:testing_descriptor_pool",
        "//runtime",
        "//runtime:runtime_builder",
        "//runtime:standard_runtime_builder_factory",
        "//testing/testrunner:cel_expression_source",
        "//testing/testrunner:cel_test_context",
        "//testing/testrunner:cel_test_factories",
        "@com_google_absl//absl/log:absl_check",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_cel_spec//proto/cel/expr/conformance/test:suite_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "raw_expr_and_cel_file_test",
    testonly = True,
    srcs = ["raw_expr_and_cel_file_test.cc"],
    deps = [
        "//checker:type_checker_builder",
        "//common:decl",
        "//common:type",
        "//compiler",
        "//compiler:compiler_factory",
        "//compiler:standard_library",
        "//internal:status_macros",
        "//internal:testing_descriptor_pool",
        "//runtime",
        "//runtime:runtime_builder",
        "//runtime:standard_runtime_builder_factory",
        "//testing/testrunner:cel_test_context",
        "//testing/testrunner:cel_test_factories",
        "@com_google_absl//absl/log:absl_check",
        "@com_google_absl//absl/log:absl_log",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_cel_spec//proto/cel/expr/conformance/test:suite_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
    alwayslink = True,
)

cc_library(
    name = "checked_expr_user_test",
    testonly = True,
    srcs = ["checked_expr_test.cc"],
    deps = [
        "//internal:status_macros",
        "//internal:testing_descriptor_pool",
        "//runtime",
        "//runtime:runtime_builder",
        "//runtime:standard_runtime_builder_factory",
        "//testing/testrunner:cel_test_context",
        "//testing/testrunner:cel_test_factories",
        "@com_google_absl//absl/log:absl_check",
        "@com_google_absl//absl/log:absl_log",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_cel_spec//proto/cel/expr/conformance/test:suite_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
    alwayslink = True,
)

cel_cc_test(
    name = "simple_test",
    enable_coverage = True,
    filegroup = "//testing/testrunner/resources",
    test_data_path = "//testing/testrunner/resources",
    test_suite = "simple_tests.textproto",
    deps = [
        ":simple_user_test",
    ],
)

cel_cc_test(
    name = "simple_test_with_custom_test_suite",
    enable_coverage = True,
    filegroup = "//testing/testrunner/resources",
    test_data_path = "//testing/testrunner/resources",
    deps = [
        ":simple_user_test",
    ],
)

cel_cc_test(
    name = "raw_expression_test_with_custom_test_suite",
    enable_coverage = True,
    deps = [
        ":raw_expression_user_test",
    ],
)

cel_cc_test(
    name = "subtraction_raw_expr_test",
    cel_expr = "x - y",
    is_raw_expr = True,
    deps = [
        ":raw_expr_and_cel_file_test",
    ],
)

cel_cc_test(
    name = "subtraction_cel_file_test",
    cel_expr = "test.cel",
    test_data_path = "//testing/testrunner/resources",
    deps = [
        ":raw_expr_and_cel_file_test",
    ],
)
