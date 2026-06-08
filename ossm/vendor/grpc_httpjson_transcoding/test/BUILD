# Copyright 2016 Google Inc. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
################################################################################
#
load("@com_google_protobuf//:protobuf.bzl", "py_proto_library")
load("@rules_fuzzing//fuzzing:cc_defs.bzl", "cc_fuzz_test")

package(default_visibility = ["//visibility:public"])

cc_test(
    name = "http_template_test",
    size = "small",
    srcs = [
        "http_template_test.cc",
    ],
    linkstatic = 1,
    deps = [
        "//src:http_template",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_fuzz_test(
    name = "http_template_fuzz_test",
    srcs = [
        "http_template_fuzz_test.cc",
    ],
    corpus = glob(["http_template_fuzz_test_corpus/**"]),
    deps = [
        "//src:http_template",
    ],
)

cc_test(
    name = "path_matcher_test",
    size = "small",
    srcs = [
        "path_matcher_test.cc",
    ],
    linkstatic = 1,
    deps = [
        "//src:path_matcher",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "path_matcher_utility_test",
    size = "small",
    srcs = [
        "path_matcher_utility_test.cc",
    ],
    linkstatic = 1,
    deps = [
        "//src:path_matcher_utility",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "prefix_writer_test",
    size = "small",
    srcs = [
        "prefix_writer_test.cc",
    ],
    deps = [
        "//src:prefix_writer",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
        "@com_google_protoconverter//:testing",
    ],
)

cc_test(
    name = "request_weaver_test",
    size = "small",
    srcs = [
        "request_weaver_test.cc",
    ],
    deps = [
        "//src:request_weaver",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
        "@com_google_protoconverter//:testing",
    ],
)

cc_test(
    name = "status_error_listener_test",
    size = "small",
    srcs = [
        "status_error_listener_test.cc",
    ],
    deps = [
        "//src:status_error_listener",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

proto_library(
    name = "bookstore_proto",
    testonly = 1,
    srcs = ["bookstore.proto"],
)

cc_proto_library(
    name = "bookstore_cc_proto",
    testonly = 1,
    deps = [":bookstore_proto"],
)

cc_test(
    name = "type_helper_test",
    size = "small",
    srcs = [
        "type_helper_test.cc",
    ],
    data = [
        "testdata/bookstore_service.pb.txt",
    ],
    deps = [
        ":test_common",
        "//src:type_helper",
        "@com_google_googleapis//google/api:service_cc_proto",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "test_common",
    testonly = 1,
    srcs = ["test_common.cc"],
    hdrs = ["test_common.h"],
    deps = [
        "//src:transcoder_input_stream",
        "@com_google_googleapis//google/api:service_cc_proto",
        "@com_google_googletest//:gtest",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "request_translator_test_base",
    testonly = 1,
    srcs = [
        "proto_stream_tester.cc",
        "proto_stream_tester.h",
        "request_translator_test_base.cc",
    ],
    hdrs = [
        "request_translator_test_base.h",
    ],
    deps = [
        ":bookstore_cc_proto",
        ":test_common",
        "//src:request_message_translator",
        "//src:type_helper",
        "@com_google_googleapis//google/api:service_cc_proto",
        "@com_google_googletest//:gtest",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "request_message_translator_test",
    size = "small",
    srcs = [
        "request_message_translator_test.cc",
    ],
    data = [
        "testdata/bookstore_service.pb.txt",
    ],
    deps = [
        ":bookstore_cc_proto",
        ":request_translator_test_base",
        ":test_common",
        "//src:request_message_translator",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "request_stream_translator_test",
    size = "small",
    srcs = [
        "request_stream_translator_test.cc",
    ],
    data = [
        "testdata/bookstore_service.pb.txt",
    ],
    deps = [
        ":bookstore_cc_proto",
        ":request_translator_test_base",
        "//src:request_stream_translator",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "json_request_translator_test",
    size = "small",
    srcs = [
        "json_request_translator_test.cc",
    ],
    data = [
        "testdata/bookstore_service.pb.txt",
    ],
    deps = [
        ":bookstore_cc_proto",
        ":request_translator_test_base",
        ":test_common",
        "//src:json_request_translator",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "message_reader_test",
    size = "small",
    srcs = [
        "message_reader_test.cc",
    ],
    deps = [
        ":test_common",
        "//src:message_reader",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_fuzz_test(
    name = "message_reader_fuzz_test",
    testonly = 1,
    srcs = [
        "message_reader_fuzz_test.cc",
    ],
    corpus = glob(["message_reader_fuzz_test_corpus/**"]),
    deps = [
        ":test_common",
        "//src:message_reader",
    ],
)

cc_test(
    name = "response_to_json_translator_test",
    size = "small",
    srcs = [
        "response_to_json_translator_test.cc",
    ],
    data = [
        "testdata/bookstore_service.pb.txt",
    ],
    deps = [
        ":bookstore_cc_proto",
        ":test_common",
        "//src:message_reader",
        "//src:response_to_json_translator",
        "//src:type_helper",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "message_stream_test",
    size = "small",
    srcs = [
        "message_stream_test.cc",
    ],
    deps = [
        ":test_common",
        "//src:message_stream",
        "@com_google_googletest//:gtest_main",
    ],
)
