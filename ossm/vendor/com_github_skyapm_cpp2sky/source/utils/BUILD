load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "util_lib",
    srcs = [
        "random_generator.cc",
    ],
    hdrs = [
        "base64.h",
        "buffer.h",
        "random_generator.h",
        "timer.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//cpp2sky/internal:random_generator_interface",
        "@com_google_absl//absl/strings",
    ],
)
