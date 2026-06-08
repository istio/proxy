load("@bazel_skylib//:bzl_library.bzl", "bzl_library")

package(default_visibility = ["//visibility:public"])

filegroup(
    name = "srcs",
    srcs = glob(["**"]),
    visibility = ["//java/common/rules:__pkg__"],
)

bzl_library(
    name = "impl",
    srcs = glob(["*.bzl"]),
    visibility = ["//java:__subpackages__"],
)

filegroup(
    name = "for_bazel_tests",
    testonly = 1,
    srcs = [
        "BUILD",
        ":impl",
    ],
    visibility = ["//java/common/rules:__pkg__"],
)
