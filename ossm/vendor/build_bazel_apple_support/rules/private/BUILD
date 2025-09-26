load("@bazel_skylib//:bzl_library.bzl", "bzl_library")

licenses(["notice"])

# Public bzl_library for anything that needs to depend on apple_genrule.bzl.
bzl_library(
    name = "apple_genrule",
    srcs = ["apple_genrule.bzl"],
    visibility = [
        "//rules:__pkg__",
    ],
    deps = [
        "//lib:apple_support",
        "@bazel_skylib//lib:dicts",
    ],
)

# Consumed by bazel tests.
filegroup(
    name = "for_bazel_tests",
    testonly = 1,
    srcs = glob(["**"]),
    visibility = ["//rules:__pkg__"],
)
