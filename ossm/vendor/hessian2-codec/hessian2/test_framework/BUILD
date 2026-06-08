load("//:copts.bzl", "DEFAULT_COPTS", "TEST_COPTS")

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "process_lib",
    srcs = [
        "process.h",
    ],
    copts = DEFAULT_COPTS,
    deps = [
    ],
)

cc_library(
    name = "test_common_lib",
    srcs = [
        "test_common.h",
    ],
    copts = DEFAULT_COPTS,
    deps = [
    ],
)

cc_library(
    name = "decoder_test_framework_lib",
    srcs = [
        "decoder_test_framework.h",
    ],
    copts = DEFAULT_COPTS,
    data = [
        "//test_hessian:test_framework_binary",
    ],
    deps = [
        ":process_lib",
        "//hessian2:codec_lib",
        "//hessian2/basic_codec:basic_codec_lib",
        "//hessian2/basic_codec:object_codec_lib",
        "@com_google_absl//absl/strings:str_format",
    ],
)

cc_library(
    name = "encoder_test_framework_lib",
    srcs = [
        "encoder_test_framework.h",
    ],
    copts = DEFAULT_COPTS,
    data = [
        "//test_hessian:test_framework_binary",
    ],
    deps = [
        ":process_lib",
        ":test_common_lib",
        "//hessian2:codec_lib",
        "//hessian2/basic_codec:string_codec_lib",
        "@com_google_absl//absl/strings:str_format",
    ],
)

cc_test(
    name = "process_unittests",
    srcs = [
        "process_unittests.cc",
    ],
    copts = TEST_COPTS,
    data = [
        "//test_hessian:test_framework_binary",
    ],
    deps = [
        ":process_lib",
        "@com_google_googletest//:gtest_main",
    ],
)
