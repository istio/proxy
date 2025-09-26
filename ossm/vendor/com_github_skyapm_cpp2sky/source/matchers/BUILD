load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "suffix_matcher_lib",
    srcs = ["suffix_matcher.cc"],
    hdrs = ["suffix_matcher.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//cpp2sky/internal:matcher_interface",
        "@com_google_absl//absl/strings",
    ],
)
