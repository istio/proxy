load("@bazel_skylib//:bzl_library.bzl", "bzl_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

filegroup(
    name = "srcs",
    srcs = glob(["**"]) + [
        "//java/common/rules:srcs",
    ],
    visibility = ["//java:__pkg__"],
)

bzl_library(
    name = "common",
    srcs = glob(["*.bzl"]),
    visibility = ["//visibility:public"],
    deps = ["//java/private"],
)

filegroup(
    name = "for_bazel_tests",
    testonly = 1,
    srcs = [
        "BUILD",
        ":common",
        "//java/common/rules:for_bazel_tests",
    ],
    visibility = ["//java:__pkg__"],
)
