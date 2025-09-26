"""Generated definition of rust_tonic_grpc_compile."""

load(
    "//:defs.bzl",
    "ProtoPluginInfo",
    "proto_compile_attrs",
)
load(":common.bzl", "ProstProtoInfo", "rust_prost_proto_compile_impl")

# Create compile rule
rust_tonic_grpc_compile = rule(
    implementation = rust_prost_proto_compile_impl,
    attrs = dict(
        proto_compile_attrs,
        prost_proto_deps = attr.label_list(
            providers = [ProstProtoInfo],
            mandatory = False,
            doc = "Other protos compiled by prost that our proto directly depends upon. Used to generated externs_path=... options for prost.",
        ),
        declared_proto_packages = attr.string_list(
            mandatory = True,
            doc = "List of labels that provide the ProtoInfo provider (such as proto_library from rules_proto)",
        ),
        crate_name = attr.string(
            mandatory = False,
            doc = "Name of the crate these protos will be compiled into later using rust_library. See rust_prost_proto_library macro for more details.",
        ),
        _plugins = attr.label_list(
            providers = [ProtoPluginInfo],
            default = [
                Label("//rust:rust_prost_plugin"),
                Label("//rust:rust_crate_plugin"),
                Label("//rust:rust_serde_plugin"),
                Label("//rust:rust_tonic_plugin"),
            ],
            doc = "List of protoc plugins to apply",
        ),
    ),
    toolchains = [str(Label("//protobuf:toolchain_type"))],
)
