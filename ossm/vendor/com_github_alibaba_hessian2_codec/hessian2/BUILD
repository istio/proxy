load("//:copts.bzl", "DEFAULT_COPTS", "TEST_COPTS")

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "byte_order_lib",
    srcs = [
        "byte_order.h",
    ],
    copts = DEFAULT_COPTS,
    deps = [
    ],
)

cc_library(
    name = "object_lib",
    srcs = [
        "object.hpp",
    ],
    copts = DEFAULT_COPTS,
    deps = [
        "//common:common_lib",
        "//common:macros_lib",
        "@com_google_absl//absl/container:node_hash_map",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/types:optional",
    ],
)

cc_library(
    name = "reader_lib",
    srcs = [
        "reader.cc",
        "reader.hpp",
    ],
    copts = DEFAULT_COPTS,
    deps = [
        ":byte_order_lib",
        "//common:common_lib",
    ],
)

cc_library(
    name = "writer_lib",
    srcs = [
        "writer.hpp",
    ],
    copts = DEFAULT_COPTS,
    deps = [
        ":byte_order_lib",
        "//common:common_lib",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "string_reader_lib",
    srcs = [
        "string_reader.hpp",
    ],
    copts = DEFAULT_COPTS,
    deps = [
        ":reader_lib",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "string_writer_lib",
    srcs = [
        "string_writer.hpp",
    ],
    copts = DEFAULT_COPTS,
    deps = [
        ":writer_lib",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "string_reader_unittests",
    srcs = [
        "string_reader_unittests.cc",
    ],
    copts = TEST_COPTS,
    deps = [
        ":string_reader_lib",
        "//common:common_lib",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "string_writer_unittests",
    srcs = [
        "string_writer_unittests.cc",
    ],
    copts = TEST_COPTS,
    deps = [
        ":string_writer_lib",
        "//common:common_lib",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "codec_impl_lib",
    srcs = [
        "codec.cc",
    ],
    copts = DEFAULT_COPTS,
    deps = [
        ":codec_lib",
        "//hessian2/basic_codec:def_ref_codec_lib",
        "//hessian2/basic_codec:type_ref_lib",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:optional",
    ],
)

cc_library(
    name = "codec_lib",
    srcs = [
        "codec.hpp",
    ],
    copts = DEFAULT_COPTS,
    deps = [
        ":object_lib",
        ":string_reader_lib",
        ":string_writer_lib",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:optional",
    ],
)

cc_test(
    name = "object_lib_test",
    srcs = [
        "object_unittests.cc",
    ],
    copts = TEST_COPTS,
    deps = [
        ":object_lib",
        "@com_google_googletest//:gtest_main",
    ],
)
