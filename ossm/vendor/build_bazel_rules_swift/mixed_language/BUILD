load("@bazel_skylib//:bzl_library.bzl", "bzl_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

bzl_library(
    name = "mixed_language",
    deps = [
        ":mixed_language_library",
    ],
)

bzl_library(
    name = "mixed_language_library",
    srcs = ["mixed_language_library.bzl"],
    deps = [
        "//mixed_language/internal:library",
        "//mixed_language/internal:module_map",
        "//mixed_language/internal:umbrella_header",
        "//swift:module_name",
        "//swift:swift_interop_hint",
        "//swift:swift_library",
        "//swift/internal:compiling",
        "@bazel_skylib//lib:paths",
    ],
)
