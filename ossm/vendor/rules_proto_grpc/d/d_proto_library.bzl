"""Generated definition of d_proto_library."""

load("//d:d_proto_compile.bzl", "d_proto_compile")
load("//:defs.bzl", "bazel_build_rule_common_attrs", "proto_compile_attrs")
load("@io_bazel_rules_d//d:d.bzl", "d_library")

def d_proto_library(name, **kwargs):
    # Compile protos
    name_pb = name + "_pb"
    d_proto_compile(
        name = name_pb,
        **{
            k: v
            for (k, v) in kwargs.items()
            if k in proto_compile_attrs.keys() or
               k in bazel_build_rule_common_attrs
        }  # Forward args
    )

    # Create d library
    d_library(
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
    "@com_github_dcarp_protobuf_d//:protobuf",
]
