load("@bazel_skylib//:bzl_library.bzl", "bzl_library")

package(default_visibility = ["//test:__subpackages__"])

bzl_library(
    name = "starlark_tests_bzls",
    srcs = glob(["*.bzl"]),
    deps = [
        "//swift:providers",
        "//swift:swift_clang_module_aspect",
    ],
)

bzl_library(
    name = "common",
    srcs = ["common.bzl"],
    deps = [
        "//swift:providers",
        "//swift:swift_clang_module_aspect",
    ],
)
