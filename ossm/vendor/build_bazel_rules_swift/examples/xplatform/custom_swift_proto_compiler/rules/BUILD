load("@bazel_skylib//:bzl_library.bzl", "bzl_library")

licenses(["notice"])

sh_binary(
    name = "custom_proto_compiler_plugin",
    srcs = ["custom_proto_compiler_plugin.sh"],
    visibility = ["//examples/xplatform/custom_swift_proto_compiler/rules:__subpackages__"],
)

sh_binary(
    name = "custom_proto_compiler",
    srcs = ["custom_proto_compiler.sh"],
    data = [":custom_proto_compiler_plugin"],
    visibility = ["//examples/xplatform/custom_swift_proto_compiler/rules:__subpackages__"],
    deps = ["@bazel_tools//tools/bash/runfiles"],
)

bzl_library(
    name = "custom_swift_proto_compiler",
    srcs = ["custom_swift_proto_compiler.bzl"],
    visibility = ["//visibility:public"],
    deps = ["@bazel_skylib//lib:paths"],
)
