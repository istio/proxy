# Copyright 2022 Google LLC
#
# Use of this source code is governed by an MIT-style
# license that can be found in the LICENSE file or at
# https://opensource.org/licenses/MIT.

# An example that output all of the possible outputs of the OCPDiag results library.

cc_library(
    name = "full_spec_lib",
    srcs = ["full_spec.cc"],
    hdrs = ["full_spec.h"],
    deps = [
        "//ocpdiag/core/results:artifact_writer",
        "//ocpdiag/core/results:measurement_series",
        "//ocpdiag/core/results:test_run",
        "//ocpdiag/core/results:test_step",
        "//ocpdiag/core/results/data_model:dut_info",
        "//ocpdiag/core/results/data_model:input_model",
    ],
)

cc_test(
    name = "full_spec_lib_test",
    srcs = ["full_spec_test.cc"],
    deps = [
        ":full_spec_lib",
        "//ocpdiag/core/results:output_receiver",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_binary(
    name = "full_spec",
    srcs = ["full_spec_main.cc"],
    deps = [
        ":full_spec_lib",
        "@com_google_absl//absl/flags:parse",
    ],
)
