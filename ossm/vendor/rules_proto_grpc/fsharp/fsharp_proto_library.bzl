"""Generated definition of fsharp_proto_library."""

load("//fsharp:fsharp_proto_compile.bzl", "fsharp_proto_compile")
load("//:defs.bzl", "bazel_build_rule_common_attrs", "proto_compile_attrs")
load("@io_bazel_rules_dotnet//dotnet:defs.bzl", "fsharp_library")

def fsharp_proto_library(name, **kwargs):
    # Compile protos
    name_pb = name + "_pb"
    fsharp_proto_compile(
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
        deps = PROTO_DEPS + kwargs.get("deps", []),
        **{
            k: v
            for (k, v) in kwargs.items()
            if k in bazel_build_rule_common_attrs
        }  # Forward Bazel common args
    )

PROTO_DEPS = [
    "@google.protobuf//:lib",
    "@fsharp.core//:lib",
    "@protobuf.fsharp//:lib",
    "@core_sdk_stdlib//:libraryset",
]
