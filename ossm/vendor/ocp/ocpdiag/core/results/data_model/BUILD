# Copyright 2022 Google LLC
#
# Use of this source code is governed by an MIT-style
# license that can be found in the LICENSE file or at
# https://opensource.org/licenses/MIT.

# The data model for the results library, including functions for converting back and forth between
# protobuf and struct representations.

load("@com_github_grpc_grpc//bazel:python_rules.bzl", "py_proto_library")

licenses(["notice"])

package(default_visibility = ["//visibility:public"])

proto_library(
    name = "results_proto",
    srcs = ["results.proto"],
    deps = [
        "@com_google_protobuf//:struct_proto",
        "@com_google_protobuf//:timestamp_proto",
    ],
)

cc_proto_library(
    name = "results_cc_proto",
    deps = [":results_proto"],
)

py_proto_library(
    name = "results_py_pb2",
    deps = [":results_proto"],
)

cc_library(
    name = "variant",
    hdrs = ["variant.h"],
    deps = ["@com_google_absl//absl/strings"],
)

cc_test(
    name = "variant_test",
    srcs = ["variant_test.cc"],
    deps = [
        ":variant",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "input_model",
    hdrs = ["input_model.h"],
    deps = [
        ":variant",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "input_model_helpers",
    srcs = ["input_model_helpers.cc"],
    hdrs = ["input_model_helpers.h"],
    deps = [
        ":input_model",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
    ],
)

cc_test(
    name = "input_model_helpers_test",
    srcs = ["input_model_helpers_test.cc"],
    deps = [
        ":input_model",
        ":input_model_helpers",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "output_model",
    hdrs = ["output_model.h"],
    deps = [
        ":input_model",
        ":variant",
        "@com_google_absl//absl/strings",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "struct_validators",
    srcs = ["struct_validators.cc"],
    hdrs = ["struct_validators.h"],
    deps = [
        ":input_model",
        ":variant",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "struct_validators_test",
    srcs = ["struct_validators_test.cc"],
    deps = [
        ":input_model",
        ":struct_validators",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "dut_info",
    srcs = ["dut_info.cc"],
    hdrs = ["dut_info.h"],
    deps = [
        ":input_model",
        ":output_model",
        ":struct_validators",
        "//ocpdiag/core/results:int_incrementer",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
    ],
)

cc_test(
    name = "dut_info_test",
    srcs = ["dut_info_test.cc"],
    deps = [
        ":dut_info",
        ":input_model",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "struct_to_proto",
    srcs = ["struct_to_proto.cc"],
    hdrs = ["struct_to_proto.h"],
    deps = [
        ":dut_info",
        ":input_model",
        ":output_model",
        ":results_cc_proto",
        ":variant",
        "//ocpdiag/core/compat:status_converters",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/strings",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "struct_to_proto_test",
    srcs = ["struct_to_proto_test.cc"],
    deps = [
        ":dut_info",
        ":input_model",
        ":results_cc_proto",
        ":struct_to_proto",
        "//ocpdiag/core/testing:proto_matchers",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "proto_to_struct",
    srcs = ["proto_to_struct.cc"],
    hdrs = ["proto_to_struct.h"],
    deps = [
        ":output_model",
        ":results_cc_proto",
        ":variant",
        "//ocpdiag/core/compat:status_converters",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/strings",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "proto_to_struct_test",
    srcs = ["proto_to_struct_test.cc"],
    deps = [
        ":output_model",
        ":proto_to_struct",
        ":results_cc_proto",
        "//ocpdiag/core/testing:parse_text_proto",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)
