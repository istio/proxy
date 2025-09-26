load("//:copts.bzl", "DEFAULT_COPTS")

package(default_visibility = ["//visibility:public"])

cc_binary(
    name = "custom_type",
    srcs = [
        "custom_type.cc",
    ],
    copts = DEFAULT_COPTS,
    deps = [
        "//hessian2:codec_impl_lib",
        "//hessian2/basic_codec:basic_codec_lib",
        "@com_google_absl//absl/strings",
    ],
)

cc_binary(
    name = "basic_type",
    srcs = [
        "basic_type.cc",
    ],
    copts = DEFAULT_COPTS,
    deps = [
        "//hessian2:codec_impl_lib",
        "//hessian2/basic_codec:basic_codec_lib",
        "@com_google_absl//absl/strings",
    ],
)

cc_binary(
    name = "reader_and_writer",
    srcs = [
        "reader_writer.cc",
    ],
    copts = DEFAULT_COPTS,
    deps = [
        "//hessian2:codec_impl_lib",
        "//hessian2:reader_lib",
        "//hessian2:writer_lib",
        "//hessian2/basic_codec:basic_codec_lib",
        "@com_google_absl//absl/strings",
    ],
)
