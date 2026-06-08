"""Generated definition of fsharp_grpc_library."""

load("//fsharp:fsharp_grpc_compile.bzl", "fsharp_grpc_compile")
load("//:defs.bzl", "bazel_build_rule_common_attrs", "proto_compile_attrs")
load("@io_bazel_rules_dotnet//dotnet:defs.bzl", "fsharp_library")

def fsharp_grpc_library(name, **kwargs):
    # Compile protos
    name_pb = name + "_pb"
    fsharp_grpc_compile(
        name = name_pb,
        **{
            k: v
            for (k, v) in kwargs.items()
            if k in proto_compile_attrs.keys() or
               k in bazel_build_rule_common_attrs
        }  # Forward args
    )

    # Create fsharp library
    fsharp_library(
        name = name,
        srcs = [name_pb],
        deps = GRPC_DEPS + kwargs.get("deps", []),
        **{
            k: v
            for (k, v) in kwargs.items()
            if k in bazel_build_rule_common_attrs
        }  # Forward Bazel common args
    )

GRPC_DEPS = [
    "@google.protobuf//:lib",
    "@grpc.net.client//:lib",
    "@grpc.aspnetcore//:lib",
    "@protobuf.fsharp//:lib",
    "@core_sdk_stdlib//:libraryset",
]
