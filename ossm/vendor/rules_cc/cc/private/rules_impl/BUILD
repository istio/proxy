load("@bazel_skylib//:bzl_library.bzl", "bzl_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])  # Apache 2.0

filegroup(
    name = "bzl_srcs",
    srcs = glob(["**/*.bzl"]) + [
        "//cc/private/rules_impl/fdo:bzl_srcs",
    ],
)

filegroup(
    name = "srcs",
    srcs = glob([
        "**/*.bzl",
        "**/BUILD",
    ]) + [
        "//cc/private/rules_impl/fdo:srcs",
    ],
)

bzl_library(
    name = "cc_flags_supplier_lib_bzl",
    srcs = ["cc_flags_supplier_lib.bzl"],
    visibility = ["//cc:__subpackages__"],
    deps = ["//cc/common"],
)

bzl_library(
    name = "failing_cc_proto_library_bzl",
    srcs = ["failing_cc_proto_library.bzl"],
    visibility = ["//cc:__subpackages__"],
    deps = ["//cc/common"],
)

bzl_library(
    name = "core_rules",
    srcs = [
        "attrs.bzl",
        "cc_binary.bzl",
        "cc_import.bzl",
        "cc_library.bzl",
        "cc_shared_library.bzl",
        "cc_static_library.bzl",
        "cc_test.bzl",
        "objc_import.bzl",
        "objc_library.bzl",
    ],
    visibility = [
        "//cc:__subpackages__",
        "@cc_compatibility_proxy//:__pkg__",
    ],
    deps = [
        ":objc_common",
        ":objc_compilation_support_bzl",
        "//cc:action_names_bzl",
        "//cc:cc_postmark_bzl",
        "//cc:find_cc_toolchain_bzl",
        "//cc/common",
        "//cc/common:cc_debug_helper_bzl",
        "//cc/common:cc_shared_library_info_bzl",
        "@bazel_skylib//lib:paths",
        "@com_google_protobuf//bazel/common:proto_info_bzl",
    ],
)

bzl_library(
    name = "toolchain_rules",
    srcs = [
        "cc_toolchain.bzl",
        "cc_toolchain_alias.bzl",
        "cc_toolchain_info.bzl",
        "cc_toolchain_provider_helper.bzl",
    ],
    visibility = [
        "//cc:__subpackages__",
        "@cc_compatibility_proxy//:__pkg__",
    ],
    deps = [
        "//cc/common",
        "//cc/private/rules_impl/fdo:fdo_rules",
        "//cc/toolchains:cc_toolchain_config_info_bzl",
        "@bazel_skylib//lib:paths",
    ],
)

bzl_library(
    name = "objc_common",
    srcs = [
        "objc_attrs.bzl",
        "objc_common.bzl",
        "objc_compilation_artifacts_info.bzl",
        "objc_compilation_context_info.bzl",
        "objc_compilation_support.bzl",
        "objc_intermediate_artifacts.bzl",
        "objc_semantics.bzl",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//cc/common",
        "@bazel_skylib//lib:paths",
    ],
)

bzl_library(
    name = "objc_compilation_support_bzl",
    srcs = ["objc_compilation_support.bzl"],
    visibility = ["//visibility:public"],
    deps = [
        ":objc_common",
        "//cc/common:cc_helper_bzl",
        "@bazel_skylib//lib:paths",
    ],
)

bzl_library(
    name = "native_cc_common_bzl",
    srcs = ["native_cc_common.bzl"],
    visibility = [
        "//cc:__subpackages__",
        "@cc_compatibility_proxy//:__pkg__",
    ],
)

bzl_library(
    name = "native_providers_bzl",
    srcs = ["native_providers.bzl"],
    visibility = [
        "//cc:__subpackages__",
        "@cc_compatibility_proxy//:__pkg__",
    ],
)

bzl_library(
    name = "cc_toolchain_info_bzl",
    srcs = ["cc_toolchain_info.bzl"],
    visibility = [
        "//cc:__subpackages__",
        "@cc_compatibility_proxy//:__pkg__",
    ],
)
