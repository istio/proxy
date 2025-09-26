load("//:copts.bzl", "DEFAULT_COPTS", "TEST_COPTS")

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "macros_lib",
    srcs = [
        "macros.h",
    ],
    copts = DEFAULT_COPTS,
)

cc_library(
    name = "const_singleton",
    srcs = [
        "const_singleton.h",
    ],
    copts = DEFAULT_COPTS,
)

cc_library(
    name = "common_lib",
    srcs = [
        "common.h",
    ],
    copts = DEFAULT_COPTS,
)

cc_library(
    name = "logger_lib",
    srcs = [
        "logger.h",
    ],
    copts = DEFAULT_COPTS,
    deps = [
        "@com_github_fmtlib_fmt//:fmt",
        "@com_google_absl//absl/strings",
    ],
)
