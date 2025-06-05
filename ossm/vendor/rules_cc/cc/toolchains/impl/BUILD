# This directory contains implementations of starlark functions that contain
# complex logic. The objective is to keep the rules themselves as simple as
# possible, so that we can perform very thorough testing on the implementation.

# I wanted to call it private / internal, but then buildifier complains about
# referencing it from the tests directory.

load("@bazel_skylib//:bzl_library.bzl", "bzl_library")

exports_files(
    ["documented_api.bzl"],
    visibility = ["//docs:__pkg__"],
)

bzl_library(
    name = "toolchain_impl_rules",
    srcs = glob(["*.bzl"]),
    visibility = ["//cc/toolchains:__subpackages__"],
    deps = ["//cc/common"],
)
