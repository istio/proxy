"""Generated definition of scala_proto_library."""

load("//scala:scala_proto_compile.bzl", "scala_proto_compile")
load("//:defs.bzl", "bazel_build_rule_common_attrs", "proto_compile_attrs")
load("@io_bazel_rules_scala//scala:scala.bzl", "scala_library")

def scala_proto_library(name, **kwargs):  # buildifier: disable=function-docstring
    # Compile protos
    name_pb = name + "_pb"
    scala_proto_compile(
        name = name_pb,
        **{
            k: v
            for (k, v) in kwargs.items()
            if k in proto_compile_attrs.keys() or
               k in bazel_build_rule_common_attrs
        }  # Forward args
    )

    # Create scala library
    scala_library(
        name = name,
        srcs = [name_pb],
        deps = PROTO_DEPS + kwargs.get("deps", []),
        exports = PROTO_DEPS + kwargs.get("exports", []),
        **{
            k: v
            for (k, v) in kwargs.items()
            if k in bazel_build_rule_common_attrs
        }  # Forward Bazel common args
    )

PROTO_DEPS = [
    "@rules_proto_grpc_scala_maven//:com_google_protobuf_protobuf_java",
    "@rules_proto_grpc_scala_maven//:com_thesamet_scalapb_lenses_2_12",
    "@rules_proto_grpc_scala_maven//:com_thesamet_scalapb_scalapb_runtime_2_12",
]
