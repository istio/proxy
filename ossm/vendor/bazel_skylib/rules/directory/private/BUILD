load("@bazel_skylib//:bzl_library.bzl", "bzl_library")

package(default_applicable_licenses = ["//:license"])

licenses(["notice"])

# export bzl files for the documentation
exports_files(
    glob(["*.bzl"]),
    visibility = ["//:__subpackages__"],
)

bzl_library(
    name = "glob",
    srcs = ["glob.bzl"],
    visibility = ["//rules/directory:__pkg__"],
)

bzl_library(
    name = "paths",
    srcs = ["paths.bzl"],
    visibility = ["//rules/directory:__pkg__"],
    deps = [
        "//lib:paths",
    ],
)

# The files needed for distribution
filegroup(
    name = "distribution",
    srcs = [
        "BUILD",
    ] + glob(["*.bzl"]),
    visibility = [
        "//:__pkg__",
    ],
)
