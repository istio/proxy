load("@bazel_skylib//:bzl_library.bzl", "bzl_library")

bzl_library(
    name = "private_bzl",
    srcs = [
        "sh_binary.bzl",
        "sh_executable.bzl",
        "sh_library.bzl",
        "sh_test.bzl",
    ],
    visibility = ["//shell:__pkg__"],
)
