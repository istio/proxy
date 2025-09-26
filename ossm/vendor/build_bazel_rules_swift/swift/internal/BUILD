load("@bazel_skylib//:bzl_library.bzl", "bzl_library")

licenses(["notice"])

# Indirection needed to prevent using `objc_library` outside of macOS
alias(
    name = "swizzle_absolute_xcttestsourcelocation",
    actual = select({
        "@build_bazel_apple_support//configs:apple": (
            "@build_bazel_apple_support//lib:swizzle_absolute_xcttestsourcelocation"
        ),
        "//conditions:default": ":dummy_swizzle_absolute_xcttestsourcelocation",
    }),
    visibility = ["//visibility:public"],
)

cc_library(
    name = "dummy_swizzle_absolute_xcttestsourcelocation",
)

# Starlark libraries

bzl_library(
    name = "action_names",
    srcs = ["action_names.bzl"],
    visibility = ["//swift:__subpackages__"],
)

bzl_library(
    name = "actions",
    srcs = ["actions.bzl"],
    visibility = ["//swift:__subpackages__"],
    deps = [
        ":features",
        "//swift/toolchains/config:action_config",
        "@bazel_skylib//lib:types",
    ],
)

bzl_library(
    name = "attrs",
    srcs = ["attrs.bzl"],
    visibility = [
        "//mixed_language:__subpackages__",
        "//proto:__subpackages__",
        "//swift:__subpackages__",
    ],
    deps = [
        ":providers",
        "@bazel_skylib//lib:dicts",
    ],
)

bzl_library(
    name = "autolinking",
    srcs = ["autolinking.bzl"],
    visibility = ["//swift:__subpackages__"],
    deps = [
        ":action_names",
        ":actions",
        "//swift/toolchains/config:action_config",
    ],
)

bzl_library(
    name = "build_settings",
    srcs = ["build_settings.bzl"],
    visibility = ["//swift:__subpackages__"],
    deps = ["@bazel_skylib//rules:common_settings"],
)

bzl_library(
    name = "compiling",
    srcs = ["compiling.bzl"],
    visibility = [
        "//mixed_language:__subpackages__",
        "//proto:__subpackages__",
        "//swift:__subpackages__",
    ],
    deps = [
        ":action_names",
        ":actions",
        ":debugging",
        ":explicit_module_map_file",
        ":feature_names",
        ":features",
        ":module_maps",
        ":utils",
        ":vfsoverlay",
        ":wmo",
        "//swift:providers",
        "@bazel_skylib//lib:paths",
        "@bazel_skylib//lib:sets",
    ],
)

bzl_library(
    name = "debugging",
    srcs = ["debugging.bzl"],
    visibility = ["//swift:__subpackages__"],
    deps = [
        ":action_names",
        ":actions",
        ":feature_names",
        ":features",
        "@bazel_skylib//lib:paths",
    ],
)

bzl_library(
    name = "developer_dirs",
    srcs = ["developer_dirs.bzl"],
    visibility = ["//swift:__subpackages__"],
    deps = ["@bazel_skylib//lib:paths"],
)

bzl_library(
    name = "env_expansion",
    srcs = ["env_expansion.bzl"],
    visibility = ["//swift:__subpackages__"],
)

bzl_library(
    name = "explicit_module_map_file",
    srcs = ["explicit_module_map_file.bzl"],
    visibility = ["//swift:__subpackages__"],
)

bzl_library(
    name = "feature_names",
    srcs = ["feature_names.bzl"],
    visibility = [
        "//mixed_language:__subpackages__",
        "//swift:__subpackages__",
    ],
)

bzl_library(
    name = "features",
    srcs = ["features.bzl"],
    visibility = [
        "//mixed_language:__subpackages__",
        "//swift:__subpackages__",
    ],
    deps = [
        ":feature_names",
        ":package_specs",
        ":target_triples",
        "@bazel_skylib//lib:collections",
        "@bazel_skylib//lib:new_sets",
        "@bazel_skylib//rules:common_settings",
    ],
)

bzl_library(
    name = "linking",
    srcs = ["linking.bzl"],
    visibility = [
        "//proto:__subpackages__",
        "//swift:__subpackages__",
    ],
    deps = [
        ":action_names",
        ":actions",
        ":attrs",
        ":autolinking",
        ":debugging",
        ":developer_dirs",
        ":feature_names",
        ":features",
        ":utils",
        "//swift:swift_clang_module_aspect",
        "@bazel_skylib//lib:collections",
        "@bazel_skylib//lib:dicts",
        "@bazel_tools//tools/build_defs/cc:action_names.bzl",
    ],
)

bzl_library(
    name = "module_maps",
    srcs = ["module_maps.bzl"],
    visibility = [
        "//mixed_language:__subpackages__",
        "//swift:__subpackages__",
    ],
    deps = ["@bazel_skylib//lib:paths"],
)

bzl_library(
    name = "output_groups",
    srcs = ["output_groups.bzl"],
    visibility = [
        "//proto:__subpackages__",
        "//swift:__subpackages__",
    ],
)

bzl_library(
    name = "package_specs",
    srcs = ["package_specs.bzl"],
    visibility = ["//swift:__subpackages__"],
)

bzl_library(
    name = "providers",
    srcs = ["providers.bzl"],
    visibility = [
        "//swift:__subpackages__",
    ],
)

bzl_library(
    name = "swift_autoconfiguration",
    srcs = ["swift_autoconfiguration.bzl"],
    visibility = ["//swift:__subpackages__"],
    deps = [
        ":toolchain_utils",
        "//swift/internal:feature_names",
    ],
)

bzl_library(
    name = "swift_interop_info",
    srcs = ["swift_interop_info.bzl"],
    visibility = ["//swift:__subpackages__"],
)

bzl_library(
    name = "swift_symbol_graph_aspect",
    srcs = ["swift_symbol_graph_aspect.bzl"],
    visibility = ["//swift:__subpackages__"],
    deps = [
        ":attrs",
        ":features",
        ":providers",
        ":symbol_graph_extracting",
        ":toolchain_utils",
        ":utils",
        "@bazel_skylib//lib:dicts",
    ],
)

bzl_library(
    name = "symbol_graph_extracting",
    srcs = ["symbol_graph_extracting.bzl"],
    visibility = ["//swift:__subpackages__"],
    deps = [
        ":action_names",
        ":actions",
        ":utils",
        "//swift:providers",
    ],
)

bzl_library(
    name = "target_triples",
    srcs = ["target_triples.bzl"],
    visibility = ["//swift:__subpackages__"],
)

bzl_library(
    name = "toolchain_utils",
    srcs = ["toolchain_utils.bzl"],
    visibility = [
        "//mixed_language:__subpackages__",
        "//proto:__subpackages__",
        "//swift:__subpackages__",
    ],
)

bzl_library(
    name = "utils",
    srcs = ["utils.bzl"],
    visibility = [
        "//mixed_language:__subpackages__",
        "//proto:__subpackages__",
        "//swift:__subpackages__",
    ],
    deps = ["@bazel_skylib//lib:paths"],
)

bzl_library(
    name = "wmo",
    srcs = ["wmo.bzl"],
    visibility = ["//swift:__subpackages__"],
    deps = [
        ":feature_names",
    ],
)

bzl_library(
    name = "vfsoverlay",
    srcs = ["vfsoverlay.bzl"],
    visibility = ["//swift:__subpackages__"],
)

# Consumed by Bazel integration tests.
filegroup(
    name = "for_bazel_tests",
    testonly = 1,
    srcs = glob(["**"]) + [
        # We should be depending on a filegroup here that represents this file
        # and its dependencies, but it doesn't exist yet.
        "@bazel_tools//tools/cpp:toolchain_utils.bzl",
        "@bazel_tools//tools/build_defs/cc:action_names_test_files",
    ],
    visibility = [
        "//swift:__pkg__",
    ],
)
