load("@bazel_skylib//:bzl_library.bzl", "bzl_library")

licenses(["notice"])

bzl_library(
    name = "swift_toolchain",
    srcs = ["swift_toolchain.bzl"],
    visibility = ["//swift:__subpackages__"],
    deps = [
        "//swift:providers",
        "//swift/internal:action_names",
        "//swift/internal:attrs",
        "//swift/internal:autolinking",
        "//swift/internal:debugging",
        "//swift/internal:feature_names",
        "//swift/internal:features",
        "//swift/internal:providers",
        "//swift/internal:target_triples",
        "//swift/internal:utils",
        "//swift/internal:wmo",
        "//swift/toolchains/config:action_config",
        "//swift/toolchains/config:all_actions_config",
        "//swift/toolchains/config:compile_config",
        "//swift/toolchains/config:symbol_graph_config",
        "//swift/toolchains/config:tool_config",
        "@bazel_skylib//lib:dicts",
        "@bazel_skylib//lib:paths",
        "@bazel_skylib//rules:common_settings",
        "@bazel_tools//tools/cpp:toolchain_utils.bzl",
    ],
)

bzl_library(
    name = "xcode_swift_toolchain",
    srcs = ["xcode_swift_toolchain.bzl"],
    visibility = ["//swift:__subpackages__"],
    deps = [
        "//swift:providers",
        "//swift/internal:action_names",
        "//swift/internal:attrs",
        "//swift/internal:feature_names",
        "//swift/internal:features",
        "//swift/internal:providers",
        "//swift/internal:target_triples",
        "//swift/internal:utils",
        "//swift/internal:wmo",
        "//swift/toolchains/config:action_config",
        "//swift/toolchains/config:all_actions_config",
        "//swift/toolchains/config:compile_config",
        "//swift/toolchains/config:compile_module_interface_config",
        "//swift/toolchains/config:symbol_graph_config",
        "//swift/toolchains/config:tool_config",
        "@bazel_skylib//lib:dicts",
        "@bazel_skylib//lib:paths",
        "@bazel_skylib//rules:common_settings",
        "@bazel_tools//tools/cpp:toolchain_utils.bzl",
    ],
)

# Consumed by Bazel integration tests.
filegroup(
    name = "for_bazel_tests",
    testonly = 1,
    srcs = glob(["**"]) + [
        "//swift/toolchains/config:for_bazel_tests",
    ],
    visibility = [
        "//swift:__pkg__",
    ],
)
