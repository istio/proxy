load("@bazel_skylib//:bzl_library.bzl", "bzl_library")

package(
    default_visibility = [
        "@rules_python//:__subpackages__",
    ]
)

bzl_library(
    name = "rules_python_config_bzl",
    srcs = ["rules_python_config.bzl"]
)

bzl_library(
    name = "py_internal_bzl",
    srcs = ["py_internal.bzl"],
    deps = ["@rules_python//tools/build_defs/python/private:py_internal_renamed_bzl"],
)
