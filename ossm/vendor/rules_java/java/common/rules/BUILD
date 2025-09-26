load("@bazel_skylib//:bzl_library.bzl", "bzl_library")

package(default_visibility = ["//visibility:public"])

filegroup(
    name = "srcs",
    srcs = glob(["**"]) + [
        "//java/common/rules/impl:srcs",
    ],
    visibility = ["//java/common:__pkg__"],
)

bzl_library(
    name = "android_lint_bzl",
    srcs = ["android_lint.bzl"],
    visibility = ["//visibility:private"],
)

bzl_library(
    name = "rule_util_bzl",
    srcs = ["rule_util.bzl"],
    visibility = ["//visibility:private"],
)

bzl_library(
    name = "core_rules",
    srcs = [
        "basic_java_library.bzl",
        "java_binary.bzl",
        "java_binary_wrapper.bzl",
        "java_import.bzl",
        "java_library.bzl",
        "java_plugin.bzl",
    ],
    visibility = [
        "//java:__subpackages__",
    ],
    deps = [
        ":android_lint_bzl",
        ":rule_util_bzl",
        "//java/common",
        "@bazel_skylib//lib:paths",
        "@rules_cc//cc/common",
    ],
)

bzl_library(
    name = "toolchain_rules",
    srcs = [
        "java_package_configuration.bzl",
        "java_runtime.bzl",
        "java_toolchain.bzl",
    ],
    visibility = [
        "//java:__subpackages__",
    ],
)

filegroup(
    name = "for_bazel_tests",
    testonly = 1,
    srcs = [
        "BUILD",
        ":core_rules",
        ":toolchain_rules",
        "//java/common/rules/impl:for_bazel_tests",
        "@rules_cc//cc/private/rules_impl:srcs",
    ],
    visibility = ["//java/common:__pkg__"],
)
