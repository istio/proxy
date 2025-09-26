"""Generated definition of js_grpc_node_library."""

load("//js:js_grpc_node_compile.bzl", "js_grpc_node_compile")
load("//:defs.bzl", "bazel_build_rule_common_attrs", "proto_compile_attrs")
load("@build_bazel_rules_nodejs//:index.bzl", "js_library")

def js_grpc_node_library(name, **kwargs):
    # Compile protos
    name_pb = name + "_pb"
    js_grpc_node_compile(
        name = name_pb,
        **{
            k: v
            for (k, v) in kwargs.items()
            if k in proto_compile_attrs.keys() or
               k in bazel_build_rule_common_attrs
        }  # Forward args
    )

    # Resolve deps
    deps = [
        dep.replace("@npm", kwargs.get("deps_repo", "@npm"))
        for dep in GRPC_DEPS
    ]

    # Create js library
    js_library(
        name = name,
        srcs = [name_pb],
        deps = deps + kwargs.get("deps", []),
        package_name = kwargs.get("package_name", name),
        strip_prefix = name_pb if not kwargs.get("legacy_path") else None,
        **{
            k: v
            for (k, v) in kwargs.items()
            if k in bazel_build_rule_common_attrs
        }  # Forward Bazel common args
    )

GRPC_DEPS = [
    "@npm//google-protobuf",
    "@npm//@grpc/grpc-js",
]
