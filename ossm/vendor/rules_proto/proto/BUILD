load("@bazel_skylib//:bzl_library.bzl", "bzl_library")

bzl_library(
    name = "defs",
    srcs = [
        "defs.bzl",
    ],
    visibility = ["//visibility:public"],
    deps = [
        ":proto_common",
        ":proto_lang_toolchain",
        ":proto_toolchain",
        "//proto/private/rules:proto_descriptor_set",
        "@com_google_protobuf//bazel:proto_library_bzl",
    ],
)

bzl_library(
    name = "repositories",
    srcs = ["repositories.bzl"],
    visibility = ["//visibility:public"],
)

bzl_library(
    name = "proto_lang_toolchain",
    srcs = [
        "proto_lang_toolchain.bzl",
    ],
    deps = [
        "@com_google_protobuf//bazel/common:proto_lang_toolchain_info_bzl",
        "@com_google_protobuf//bazel/toolchains:proto_lang_toolchain_bzl",
    ],
)

bzl_library(
    name = "proto_common",
    srcs = [
        "proto_common.bzl",
    ],
    deps = [
        "@com_google_protobuf//bazel/common:proto_common_bzl",
    ],
)

bzl_library(
    name = "proto_toolchain",
    srcs = [
        "proto_toolchain.bzl",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "@com_google_protobuf//bazel/toolchains:proto_toolchain_bzl",
    ],
)

# Toolchain type provided by proto_toolchain rule and used by proto_library
alias(
    name = "toolchain_type",
    actual = "@com_google_protobuf//bazel/private:proto_toolchain_type",
    visibility = ["//visibility:public"],
)
