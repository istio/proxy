"""Generated definition of go_validate_library."""

load("//go:go_grpc_library.bzl", "GRPC_DEPS")
load("//go:go_validate_compile.bzl", "go_validate_compile")
load("//:defs.bzl", "bazel_build_rule_common_attrs", "proto_compile_attrs")
load("@io_bazel_rules_go//go:def.bzl", "go_library")

def go_validate_library(name, **kwargs):
    # Compile protos
    name_pb = name + "_pb"
    go_validate_compile(
        name = name_pb,
        prefix_path = kwargs.get("prefix_path", kwargs.get("importpath", "")),
        **{
            k: v
            for (k, v) in kwargs.items()
            if (k in proto_compile_attrs.keys() and k != "prefix_path") or
               k in bazel_build_rule_common_attrs
        }  # Forward args
    )

    # Create go library
    go_library(
        name = name,
        srcs = [name_pb],
        deps = kwargs.get("go_deps", []) + VALIDATE_DEPS + kwargs.get("deps", []),
        importpath = kwargs.get("importpath"),
        **{
            k: v
            for (k, v) in kwargs.items()
            if k in bazel_build_rule_common_attrs
        }  # Forward Bazel common args
    )

VALIDATE_DEPS = [
    "@com_github_envoyproxy_protoc_gen_validate//validate:go_default_library",
] + GRPC_DEPS
