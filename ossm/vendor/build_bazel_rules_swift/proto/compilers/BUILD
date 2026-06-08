load("@bazel_skylib//:bzl_library.bzl", "bzl_library")
load("//proto:swift_proto_compiler.bzl", "swift_proto_compiler")
load(
    "//proto/compilers:swift_proto_compiler_macros.bzl",
    "GRPC_VARIANT_CLIENT",
    "GRPC_VARIANT_SERVER",
    "GRPC_VARIANT_TEST_CLIENT",
    "PROTO_PLUGIN_OPTIONS",
    "PROTO_PLUGIN_OPTION_ALLOWLIST",
    "make_grpc_swift_proto_compiler",
)

package(default_visibility = ["//visibility:public"])

# swift_proto_compiler

swift_proto_compiler(
    name = "swift_proto",
    plugin = "//tools/protoc_wrapper:ProtoCompilerPlugin",
    plugin_name = "swift",
    plugin_option_allowlist = PROTO_PLUGIN_OPTION_ALLOWLIST,
    plugin_options = PROTO_PLUGIN_OPTIONS,
    protoc = "//tools/protoc_wrapper:protoc",
    suffixes = [".pb.swift"],
    visibility = ["//visibility:public"],
    deps = [
        "@com_github_apple_swift_protobuf//:SwiftProtobuf",
    ],
)

# Swift GRPC

make_grpc_swift_proto_compiler(
    name = "swift_server_proto",
    variants = [GRPC_VARIANT_SERVER],
)

make_grpc_swift_proto_compiler(
    name = "swift_client_proto",
    variants = [GRPC_VARIANT_CLIENT],
)

make_grpc_swift_proto_compiler(
    name = "swift_test_client_proto",
    variants = [GRPC_VARIANT_TEST_CLIENT],
)

bzl_library(
    name = "swift_proto_compiler_macros",
    srcs = ["swift_proto_compiler_macros.bzl"],
    visibility = ["//visibility:public"],
    deps = [
        "//proto:swift_proto_compiler",
        "@bazel_skylib//lib:dicts",
    ],
)
