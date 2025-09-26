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
    name = "expr_printer",
    srcs = ["expr_printer.cc"],
    hdrs = ["expr_printer.h"],
    deps = [
        "//common:ast",
        "//common:constant",
        "//common:expr",
        "//common/ast:ast_impl",
        "//extensions/protobuf:ast_converters",
        "//internal:strings",
        "@com_google_absl//absl/base:no_destructor",
        "@com_google_absl//absl/log:absl_log",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_cel_spec//proto/cel/expr:syntax_cc_proto",
    ],
)

cc_test(
    name = "expr_printer_test",
    srcs = ["expr_printer_test.cc"],
    deps = [
        ":expr_printer",
        "//common:expr",
        "//internal:testing",
        "//parser",
        "//parser:options",
        "@com_google_absl//absl/base:no_destructor",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "util",
    testonly = True,
    hdrs = [
        "util.h",
    ],
    deps = ["//internal:proto_matchers"],
)

cc_library(
    name = "baseline_tests",
    testonly = True,
    srcs = ["baseline_tests.cc"],
    hdrs = ["baseline_tests.h"],
    deps = [
        ":expr_printer",
        "//common:ast",
        "//common:expr",
        "//common/ast:ast_impl",
        "//common/ast:expr",
        "//extensions/protobuf:ast_converters",
        "@com_google_absl//absl/strings",
        "@com_google_cel_spec//proto/cel/expr:checked_cc_proto",
    ],
)

cc_test(
    name = "baseline_tests_test",
    srcs = ["baseline_tests_test.cc"],
    deps = [
        ":baseline_tests",
        "//common/ast:ast_impl",
        "//common/ast:expr",
        "//internal:testing",
        "@com_google_protobuf//:protobuf",
    ],
)
