# Build settings used throughout rules_apple build rules.

load("@bazel_skylib//:bzl_library.bzl", "bzl_library")
load("@bazel_skylib//rules:common_settings.bzl", "bool_flag", "string_flag")
load(
    "//apple/build_settings:build_settings.bzl",
    "build_settings",
)

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

[
    bool_flag(
        name = build_setting_name,
        build_setting_default = build_setting_info.default,
    )
    for build_setting_name, build_setting_info in build_settings.items()
    if type(build_setting_info.default) == "bool"
]

[
    string_flag(
        name = build_setting_name,
        build_setting_default = build_setting_info.default,
    )
    for build_setting_name, build_setting_info in build_settings.items()
    if type(build_setting_info.default) == "string"
]

bzl_library(
    name = "build_settings",
    srcs = ["build_settings.bzl"],
    visibility = [
        "//:__subpackages__",
    ],
)

# Consumed by bazel tests.
filegroup(
    name = "for_bazel_tests",
    testonly = 1,
    srcs = glob(["**"]),
    visibility = [
        "//apple:__subpackages__",
    ],
)
