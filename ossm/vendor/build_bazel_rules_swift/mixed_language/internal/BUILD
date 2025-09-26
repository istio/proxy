load("@bazel_skylib//:bzl_library.bzl", "bzl_library")

licenses(["notice"])

bzl_library(
    name = "library",
    srcs = ["library.bzl"],
    visibility = ["//mixed_language:__subpackages__"],
    deps = [
        "//swift:providers",
        "//swift:swift_clang_module_aspect",
        "//swift/internal:attrs",
        "//swift/internal:feature_names",
        "//swift/internal:features",
        "//swift/internal:toolchain_utils",
        "//swift/internal:utils",
        "@bazel_skylib//lib:dicts",
        "@bazel_skylib//lib:paths",
    ],
)

bzl_library(
    name = "module_map",
    srcs = ["module_map.bzl"],
    visibility = ["//mixed_language:__subpackages__"],
    deps = [
        "//swift/internal:module_maps",
    ],
)

bzl_library(
    name = "umbrella_header",
    srcs = ["umbrella_header.bzl"],
    visibility = ["//mixed_language:__subpackages__"],
    deps = [
        "@bazel_skylib//lib:paths",
    ],
)
