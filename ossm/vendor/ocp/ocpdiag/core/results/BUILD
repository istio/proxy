# Copyright 2022 Google LLC
#
# Use of this source code is governed by an MIT-style
# license that can be found in the LICENSE file or at
# https://opensource.org/licenses/MIT.

licenses(["notice"])

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "int_incrementer",
    hdrs = ["int_incrementer.h"],
    deps = ["@com_google_absl//absl/synchronization"],
)

cc_library(
    name = "test_result_calculator",
    srcs = ["test_result_calculator.cc"],
    hdrs = ["test_result_calculator.h"],
    deps = [
        "//ocpdiag/core/results/data_model:output_model",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/synchronization",
    ],
)

cc_test(
    name = "test_result_calculator_test",
    srcs = [
        "test_result_calculator_test.cc",
    ],
    deps = [
        ":test_result_calculator",
        "//ocpdiag/core/results/data_model:results_cc_proto",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "artifact_writer",
    srcs = ["artifact_writer.cc"],
    hdrs = ["artifact_writer.h"],
    deps = [
        ":int_incrementer",
        "//ocpdiag/core/compat:status_converters",
        "//ocpdiag/core/results/data_model:results_cc_proto",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_protobuf//:protobuf",
        "@com_google_riegeli//riegeli/base:object",
        "@com_google_riegeli//riegeli/bytes:fd_writer",
        "@com_google_riegeli//riegeli/records:record_writer",
        "@com_google_riegeli//riegeli/records:records_metadata_cc_proto",
    ],
)

cc_test(
    name = "artifact_writer_test",
    srcs = [
        "artifact_writer_test.cc",
    ],
    deps = [
        ":artifact_writer",
        "//ocpdiag/core/results/data_model:results_cc_proto",
        "//ocpdiag/core/testing:file_utils",
        "//ocpdiag/core/testing:proto_matchers",
        "//ocpdiag/core/testing:status_matchers",
        "@com_google_absl//absl/cleanup",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
        "@com_google_riegeli//riegeli/bytes:fd_reader",
        "@com_google_riegeli//riegeli/records:record_reader",
    ],
)

cc_library(
    name = "output_iterator",
    hdrs = ["output_iterator.h"],
    deps = [
        "//ocpdiag/core/results/data_model:output_model",
        "//ocpdiag/core/results/data_model:proto_to_struct",
        "//ocpdiag/core/results/data_model:results_cc_proto",
        "@com_google_absl//absl/log:check",
        "@com_google_riegeli//riegeli/bytes:fd_reader",
        "@com_google_riegeli//riegeli/records:record_reader",
    ],
)

cc_test(
    name = "output_iterator_test",
    srcs = ["output_iterator_test.cc"],
    deps = [
        ":output_iterator",
        "//ocpdiag/core/results/data_model:results_cc_proto",
        "//ocpdiag/core/testing:file_utils",
        "//ocpdiag/core/testing:parse_text_proto",
        "@com_google_googletest//:gtest_main",
        "@com_google_riegeli//riegeli/bytes:fd_writer",
        "@com_google_riegeli//riegeli/records:record_writer",
    ],
)

cc_library(
    name = "output_receiver",
    testonly = 1,
    srcs = ["output_receiver.cc"],
    hdrs = ["output_receiver.h"],
    deps = [
        ":artifact_writer",
        ":output_iterator",
        "//ocpdiag/core/results/data_model:output_model",
        "//ocpdiag/core/results/data_model:results_cc_proto",
        "//ocpdiag/core/testing:file_utils",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/log:check",
    ],
)

cc_test(
    name = "output_receiver_test",
    srcs = ["output_receiver_test.cc"],
    deps = [
        ":artifact_writer",
        ":output_receiver",
        "//ocpdiag/core/results/data_model:output_model",
        "//ocpdiag/core/results/data_model:proto_to_struct",
        "//ocpdiag/core/results/data_model:results_cc_proto",
        "//ocpdiag/core/testing:parse_text_proto",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "log_sink",
    hdrs = ["log_sink.h"],
    deps = [
        ":artifact_writer",
        "//ocpdiag/core/results/data_model:results_cc_proto",
        "@com_google_absl//absl/base:log_severity",
        "@com_google_absl//absl/log:log_entry",
        "@com_google_absl//absl/log:log_sink",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "log_sink_test",
    srcs = ["log_sink_test.cc"],
    deps = [
        ":log_sink",
        ":output_receiver",
        "//ocpdiag/core/results/data_model:input_model",
        "@com_google_absl//absl/base:log_severity",
        "@com_google_absl//absl/log",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "test_run",
    srcs = ["test_run.cc"],
    hdrs = ["test_run.h"],
    deps = [
        ":artifact_writer",
        ":int_incrementer",
        ":log_sink",
        ":test_result_calculator",
        "//ocpdiag/core/results/data_model:dut_info",
        "//ocpdiag/core/results/data_model:input_model",
        "//ocpdiag/core/results/data_model:results_cc_proto",
        "//ocpdiag/core/results/data_model:struct_to_proto",
        "//ocpdiag/core/results/data_model:struct_validators",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/log:log_sink_registry",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
    ],
)

cc_test(
    name = "test_run_test",
    srcs = ["test_run_test.cc"],
    deps = [
        ":artifact_writer",
        ":output_receiver",
        ":test_run",
        "//ocpdiag/core/results/data_model:dut_info",
        "//ocpdiag/core/results/data_model:input_model",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "test_step",
    srcs = ["test_step.cc"],
    hdrs = ["test_step.h"],
    deps = [
        ":artifact_writer",
        ":test_run",
        "//ocpdiag/core/results/data_model:input_model",
        "//ocpdiag/core/results/data_model:results_cc_proto",
        "//ocpdiag/core/results/data_model:struct_to_proto",
        "//ocpdiag/core/results/data_model:struct_validators",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
    ],
)

cc_test(
    name = "test_step_test",
    srcs = ["test_step_test.cc"],
    deps = [
        ":output_receiver",
        ":test_run",
        ":test_step",
        "//ocpdiag/core/results/data_model:dut_info",
        "//ocpdiag/core/results/data_model:input_model",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "measurement_series",
    srcs = ["measurement_series.cc"],
    hdrs = ["measurement_series.h"],
    deps = [
        ":artifact_writer",
        ":int_incrementer",
        ":test_step",
        "//ocpdiag/core/results/data_model:input_model",
        "//ocpdiag/core/results/data_model:results_cc_proto",
        "//ocpdiag/core/results/data_model:struct_to_proto",
        "//ocpdiag/core/results/data_model:struct_validators",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/synchronization",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "measurement_series_test",
    srcs = ["measurement_series_test.cc"],
    deps = [
        ":measurement_series",
        ":output_receiver",
        ":test_run",
        ":test_step",
        "//ocpdiag/core/results/data_model:dut_info",
        "//ocpdiag/core/results/data_model:input_model",
        "@com_google_absl//absl/log:check",
        "@com_google_googletest//:gtest_main",
    ],
)
