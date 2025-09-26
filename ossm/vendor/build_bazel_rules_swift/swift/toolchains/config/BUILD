load("@bazel_skylib//:bzl_library.bzl", "bzl_library")

package(
    default_visibility = [
        "//swift/internal:__pkg__",
        "//swift/toolchains:__pkg__",
    ],
)

exports_files(["const_protocols_to_gather.json"])

bzl_library(
    name = "action_config",
    srcs = ["action_config.bzl"],
    deps = [
        "@bazel_skylib//lib:types",
    ],
)

bzl_library(
    name = "all_actions_config",
    srcs = ["all_actions_config.bzl"],
    deps = [
        ":action_config",
        "//swift/internal:action_names",
    ],
)

bzl_library(
    name = "compile_config",
    srcs = ["compile_config.bzl"],
    deps = [
        ":action_config.bzl",
        "//swift/internal:action_names",
        "//swift/internal:feature_names",
        "@bazel_skylib//lib:collections",
        "@bazel_skylib//lib:paths",
        "@bazel_skylib//lib:types",
    ],
)

bzl_library(
    name = "compile_module_interface_config",
    srcs = ["compile_module_interface_config.bzl"],
    deps = [
        ":action_config",
        "//swift/internal:action_names",
    ],
)

bzl_library(
    name = "modulewrap_config",
    srcs = ["modulewrap_config.bzl"],
    deps = [
        ":action_config",
        "//swift/internal:action_names",
    ],
)

bzl_library(
    name = "symbol_graph_config",
    srcs = ["symbol_graph_config.bzl"],
    deps = [
        ":action_config",
        "//swift/internal:action_names",
    ],
)

bzl_library(
    name = "tool_config",
    srcs = ["tool_config.bzl"],
    deps = [
        "@bazel_skylib//lib:paths",
    ],
)

# Consumed by Bazel integration tests.
filegroup(
    name = "for_bazel_tests",
    testonly = 1,
    srcs = glob(["**"]),
    visibility = [
        "//swift/toolchains:__pkg__",
    ],
)
