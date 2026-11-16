load("@bazel_skylib//:bzl_library.bzl", "bzl_library")
load(":toolchain.bzl", "pigz_unavailable", "resolved_pigz")

exports_files([
    "README.md",
    "defs.bzl",
    "extensions.bzl",
    "toolchain.bzl",
])

bzl_library(
    name = "pigz_bzl",
    srcs = [
        "defs.bzl",
        "extensions.bzl",
        "toolchain.bzl",
        "//pigz/private:pigz_prebuilt.bzl",
    ],
    visibility = ["//visibility:private"],
    deps = ["//private:extension_utils_bzl"],
)

# Toolchain type for hermetic pigz implementations.
toolchain_type(
    name = "toolchain_type",
    visibility = ["//visibility:public"],
)

# Registered last: resolved where there is no other pigz toolchain, it fails with a message.
pigz_unavailable(
    name = "unavailable_impl",
)

toolchain(
    name = "unavailable_toolchain",
    toolchain = ":unavailable_impl",
    toolchain_type = ":toolchain_type",
)

# `pigz` for the platform it runs on: use it as a tool, eg
# `pkg_tar(compressor = "@envoy_toolshed//pigz", extension = "tar.gz")`.
resolved_pigz(
    name = "pigz",
    visibility = ["//visibility:public"],
)
