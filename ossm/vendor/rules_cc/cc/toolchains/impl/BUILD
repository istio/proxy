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

config_setting(
    name = "darwin_x86_64",
    constraint_values = [
        "@platforms//cpu:x86_64",
        "@platforms//os:macos",
    ],
    visibility = ["//visibility:public"],
)

config_setting(
    name = "darwin_aarch64",
    constraint_values = [
        "@platforms//cpu:aarch64",
        "@platforms//os:macos",
    ],
    visibility = ["//visibility:public"],
)

config_setting(
    name = "linux_x86_64",
    constraint_values = [
        "@platforms//cpu:x86_64",
        "@platforms//os:linux",
    ],
    visibility = ["//visibility:public"],
)

config_setting(
    name = "linux_aarch64",
    constraint_values = [
        "@platforms//cpu:aarch64",
        "@platforms//os:linux",
    ],
    visibility = ["//visibility:public"],
)

config_setting(
    name = "windows_x86_32",
    constraint_values = [
        "@platforms//os:windows",
        "@platforms//cpu:x86_32",
    ],
    visibility = ["//visibility:public"],
)

config_setting(
    name = "windows_x86_64",
    constraint_values = [
        "@platforms//os:windows",
        "@platforms//cpu:x86_64",
    ],
    visibility = ["//visibility:public"],
)
