load("@bazel_skylib//:bzl_library.bzl", "bzl_library")

bzl_library(
    name = "proto_descriptor_set",
    srcs = [
        "proto_descriptor_set.bzl",
    ],
    visibility = [
        "//proto:__subpackages__",
    ],
    deps = [
        "@com_google_protobuf//bazel/common:proto_info_bzl",
    ],
)
