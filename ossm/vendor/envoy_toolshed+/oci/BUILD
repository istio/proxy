load("@bazel_skylib//:bzl_library.bzl", "bzl_library")

exports_files(["defs.bzl"])

bzl_library(
    name = "oci_bzl",
    srcs = [
        "defs.bzl",
        "//oci/private:base_edit.bzl",
    ],
    visibility = ["//visibility:public"],
)
