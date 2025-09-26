"""Generated definition of python_grpc_library."""

load("//python:python_grpc_compile.bzl", "python_grpc_compile")
load("//:defs.bzl", "bazel_build_rule_common_attrs", "proto_compile_attrs")
load("@rules_python//python:defs.bzl", "py_library")

def python_grpc_library(name, **kwargs):
    # Compile protos
    name_pb = name + "_pb"
    python_grpc_compile(
        name = name_pb,
        **{
            k: v
            for (k, v) in kwargs.items()
            if k in proto_compile_attrs.keys() or
               k in bazel_build_rule_common_attrs
        }  # Forward args
    )

    # Create python library
    py_library(
        name = name,
        srcs = [name_pb],
        deps = GRPC_DEPS + kwargs.get("deps", []),
        data = kwargs.get("data", []),  # See https://github.com/rules-proto-grpc/rules_proto_grpc/issues/257 for use case
        imports = [name_pb],
        **{
            k: v
            for (k, v) in kwargs.items()
            if k in bazel_build_rule_common_attrs
        }  # Forward Bazel common args
    )

GRPC_DEPS = [
    "@com_google_protobuf//:protobuf_python",
    "@com_github_grpc_grpc//src/python/grpcio/grpc:grpcio",
]
