load("@bazel_skylib//:bzl_library.bzl", "bzl_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])  # Apache 2.0

filegroup(
    name = "bzl_srcs",
    srcs = glob([
        "**/*.bzl",
    ]),
)

filegroup(
    name = "srcs",
    srcs = glob([
        "**/*.bzl",
        "**/BUILD",
    ]),
)

bzl_library(
    name = "cc_flags_supplier_lib_bzl",
    srcs = ["cc_flags_supplier_lib.bzl"],
    visibility = ["//cc:__subpackages__"],
    deps = ["//cc/common"],
)

bzl_library(
    name = "native_bzl",
    srcs = ["native.bzl"],
    visibility = ["//cc:__subpackages__"],
)
