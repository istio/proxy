load("//:copts.bzl", "DEFAULT_COPTS", "TEST_COPTS")

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "bool_codec_lib",
    srcs = [
        "bool_codec.cc",
        "bool_codec.hpp",
    ],
    copts = DEFAULT_COPTS,
    deps = [
        "//hessian2:codec_lib",
    ],
)

cc_test(
    name = "bool_codec_unittests",
    srcs = [
        "bool_codec_unittests.cc",
    ],
    copts = TEST_COPTS,
    deps = [
        ":bool_codec_lib",
        "//hessian2/test_framework:decoder_test_framework_lib",
        "//hessian2/test_framework:encoder_test_framework_lib",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "byte_codec_lib",
    srcs = [
        "byte_codec.cc",
        "byte_codec.hpp",
    ],
    copts = DEFAULT_COPTS,
    deps = [
        "//hessian2:codec_lib",
    ],
)

cc_test(
    name = "byte_codec_unittests",
    srcs = [
        "byte_codec_unittests.cc",
    ],
    copts = TEST_COPTS,
    deps = [
        ":byte_codec_lib",
        "//hessian2/test_framework:decoder_test_framework_lib",
        "//hessian2/test_framework:encoder_test_framework_lib",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "date_codec_lib",
    srcs = [
        "date_codec.cc",
        "date_codec.hpp",
    ],
    copts = DEFAULT_COPTS,
    deps = [
        "//hessian2:codec_lib",
    ],
)

cc_test(
    name = "date_codec_unittests",
    srcs = [
        "date_codec_unittests.cc",
    ],
    copts = TEST_COPTS,
    deps = [
        ":date_codec_lib",
        "//hessian2/test_framework:decoder_test_framework_lib",
        "//hessian2/test_framework:encoder_test_framework_lib",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "number_codec_lib",
    srcs = [
        "number_codec.cc",
        "number_codec.hpp",
    ],
    copts = DEFAULT_COPTS,
    deps = [
        "//hessian2:codec_lib",
    ],
)

cc_test(
    name = "number_codec_unittests",
    srcs = [
        "number_codec_unittests.cc",
    ],
    copts = TEST_COPTS,
    deps = [
        ":number_codec_lib",
        "//hessian2/test_framework:decoder_test_framework_lib",
        "//hessian2/test_framework:encoder_test_framework_lib",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "string_codec_lib",
    srcs = [
        "string_codec.cc",
        "string_codec.hpp",
    ],
    copts = DEFAULT_COPTS,
    deps = [
        "//hessian2:codec_lib",
    ],
)

cc_test(
    name = "string_codec_unittests",
    srcs = [
        "string_codec_unittests.cc",
    ],
    copts = TEST_COPTS,
    deps = [
        ":string_codec_lib",
        "//hessian2/test_framework:decoder_test_framework_lib",
        "//hessian2/test_framework:encoder_test_framework_lib",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "ref_object_codec_lib",
    srcs = [
        "ref_object_codec.cc",
        "ref_object_codec.hpp",
    ],
    copts = DEFAULT_COPTS,
    deps = [
        ":number_codec_lib",
        ":string_codec_lib",
        "//hessian2:codec_lib",
    ],
)

cc_library(
    name = "type_ref_lib",
    srcs = [
        "type_ref_codec.cc",
        "type_ref_codec.hpp",
    ],
    copts = DEFAULT_COPTS,
    deps = [
        ":number_codec_lib",
        ":string_codec_lib",
        "//hessian2:codec_lib",
    ],
)

cc_test(
    name = "type_ref_codec_unittests",
    srcs = [
        "type_ref_codec_unittests.cc",
    ],
    copts = TEST_COPTS,
    deps = [
        ":type_ref_lib",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "def_ref_codec_lib",
    srcs = [
        "def_ref_codec.cc",
        "def_ref_codec.hpp",
    ],
    copts = DEFAULT_COPTS,
    deps = [
        ":number_codec_lib",
        ":string_codec_lib",
        "//hessian2:codec_lib",
    ],
)

cc_test(
    name = "def_ref_codec_unittests",
    srcs = [
        "def_ref_codec_unittests.cc",
    ],
    copts = TEST_COPTS,
    deps = [
        ":def_ref_codec_lib",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "object_codec_lib",
    srcs = [
        "class_instance_codec.cc",
        "class_instance_codec.hpp",
        "list_codec.cc",
        "list_codec.hpp",
        "map_codec.cc",
        "map_codec.hpp",
        "object_codec.cc",
        "object_codec.hpp",
    ],
    copts = DEFAULT_COPTS,
    deps = [
        ":bool_codec_lib",
        ":byte_codec_lib",
        ":date_codec_lib",
        ":def_ref_codec_lib",
        ":number_codec_lib",
        ":ref_object_codec_lib",
        ":string_codec_lib",
        ":type_ref_lib",
        "//hessian2:codec_lib",
        "//hessian2:object_lib",
    ],
)

cc_library(
    name = "basic_codec_lib",
    srcs = [
        "basic_codec.hpp",
    ],
    copts = DEFAULT_COPTS,
    deps = [
        ":bool_codec_lib",
        ":byte_codec_lib",
        ":date_codec_lib",
        ":number_codec_lib",
        ":object_codec_lib",
        ":string_codec_lib",
    ],
)

cc_test(
    name = "object_codec_unittests",
    srcs = [
        "object_codec_unittests.cc",
    ],
    copts = TEST_COPTS,
    deps = [
        ":object_codec_lib",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "list_codec_unittests",
    srcs = [
        "list_codec_unittests.cc",
    ],
    copts = TEST_COPTS,
    deps = [
        ":object_codec_lib",
        "//hessian2:object_lib",
        "//hessian2/test_framework:decoder_test_framework_lib",
        "//hessian2/test_framework:encoder_test_framework_lib",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "map_codec_unittests",
    srcs = [
        "map_codec_unittests.cc",
    ],
    copts = TEST_COPTS,
    deps = [
        ":object_codec_lib",
        "//hessian2:object_lib",
        "//hessian2/test_framework:decoder_test_framework_lib",
        "//hessian2/test_framework:encoder_test_framework_lib",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "class_instance_codec_unittests",
    srcs = [
        "class_instance_codec_unittests.cc",
    ],
    copts = TEST_COPTS,
    deps = [
        ":object_codec_lib",
        "//hessian2:object_lib",
        "//hessian2/test_framework:decoder_test_framework_lib",
        "//hessian2/test_framework:encoder_test_framework_lib",
        "@com_google_googletest//:gtest_main",
    ],
)
