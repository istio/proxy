"""Generated definition of python_grpclib_library."""

load("//python:python_grpclib_compile.bzl", "python_grpclib_compile")
load("//:defs.bzl", "bazel_build_rule_common_attrs", "proto_compile_attrs")
load("@rules_python//python:defs.bzl", "py_library")

def python_grpclib_library(name, **kwargs):
    # Compile protos
    name_pb = name + "_pb"
    python_grpclib_compile(
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
        deps = [
            "@com_google_protobuf//:protobuf_python",
        ] + GRPC_DEPS + kwargs.get("deps", []),
        data = kwargs.get("data", []),  # See https://github.com/rules-proto-grpc/rules_proto_grpc/issues/257 for use case
        imports = [name_pb],
        **{
            k: v
            for (k, v) in kwargs.items()
            if k in bazel_build_rule_common_attrs
        }  # Forward Bazel common args
    )

GRPC_DEPS = [
    # Don't use requirement(), since rules_proto_grpc_py3_deps doesn't necessarily exist when
    # imported by defs.bzl
    "@rules_proto_grpc_py3_deps_grpclib//:pkg",
]
