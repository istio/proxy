"""Generated definition of ruby_proto_library."""

load("//ruby:ruby_proto_compile.bzl", "ruby_proto_compile")
load("//:defs.bzl", "bazel_build_rule_common_attrs", "proto_compile_attrs")
load("@bazelruby_rules_ruby//ruby:defs.bzl", "ruby_library")

def ruby_proto_library(name, **kwargs):
    # Compile protos
    name_pb = name + "_pb"
    ruby_proto_compile(
        name = name_pb,
        **{
            k: v
            for (k, v) in kwargs.items()
            if k in proto_compile_attrs.keys() or
               k in bazel_build_rule_common_attrs
        }  # Forward args
    )

    # Create ruby library
    ruby_library(
        name = name,
        srcs = [name_pb],
        deps = ["@rules_proto_grpc_bundle//:gems"] + kwargs.get("deps", []),
        includes = [native.package_name() + "/" + name_pb],
        **{
            k: v
            for (k, v) in kwargs.items()
            if k in bazel_build_rule_common_attrs
        }  # Forward Bazel common args
    )
