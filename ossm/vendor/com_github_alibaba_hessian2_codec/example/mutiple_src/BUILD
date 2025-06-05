load("//:copts.bzl", "DEFAULT_COPTS", "TEST_COPTS")

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "custom_type",
    srcs = [
        "custom_type.cc",
    ],
    hdrs = [
        "custom_type.h",
    ],
    copts = DEFAULT_COPTS,
    deps = [
        "//hessian2:codec_impl_lib",
        "//hessian2/basic_codec:basic_codec_lib",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "basic_type",
    srcs = [
        "basic_type.cc",
    ],
    hdrs = [
        "basic_type.h",
    ],
    copts = DEFAULT_COPTS,
    deps = [
        "//hessian2:codec_impl_lib",
        "//hessian2/basic_codec:basic_codec_lib",
        "@com_google_absl//absl/strings",
    ],
)

cc_binary(
    name = "mutiple_src",
    srcs = [
        "mutiple_src.cc",
    ],
    copts = DEFAULT_COPTS,
    deps = [
        ":basic_type",
        ":custom_type",
    ],
)
