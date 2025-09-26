"""Generated definition of rust_prost_proto_library."""

load("//rust:common.bzl", "create_name_to_label", "prepare_prost_proto_deps", "prost_compile_attrs")
load("//rust:rust_prost_proto_compile.bzl", "rust_prost_proto_compile")
load("//:defs.bzl", "bazel_build_rule_common_attrs", "proto_compile_attrs")
load("//rust:rust_fixer.bzl", "rust_proto_crate_fixer", "rust_proto_crate_root")
load("@rules_rust//rust:defs.bzl", "rust_library")

def rust_prost_proto_library(name, **kwargs):  # buildifier: disable=function-docstring
    # Compile protos
    name_pb = name + "_pb"
    name_fixed = name_pb + "_fixed"
    name_root = name + "_root"

    prost_proto_deps = kwargs.get("prost_proto_deps", [])
    prost_proto_compiled_targets = prepare_prost_proto_deps(prost_proto_deps)

    rust_prost_proto_compile(
        name = name_pb,
        crate_name = name,
        prost_proto_deps = prost_proto_compiled_targets,
        **{
            k: v
            for (k, v) in kwargs.items()
            if k in proto_compile_attrs.keys() or
               k in bazel_build_rule_common_attrs or
               k in prost_compile_attrs
        }  # Forward args
    )

    # Fix up imports
    rust_proto_crate_fixer(
        name = name_fixed,
        compilation = name_pb,
    )

    rust_proto_crate_root(
        name = name_root,
        crate_dir = name_fixed,
    )

    # Create rust_prost library
    rust_library(
        name = name,
        edition = "2021",
        crate_root = name_root,
        crate_name = kwargs.get("crate_name"),
        srcs = [name_fixed],
        deps = kwargs.get("prost_deps", [create_name_to_label("prost"), create_name_to_label("prost-types")]) +
               kwargs.get("pbjson_deps", [create_name_to_label("pbjson-types"), create_name_to_label("pbjson")]) +
               kwargs.get("serde_deps", [create_name_to_label("serde")]) +
               kwargs.get("deps", []) +
               prost_proto_deps,
        proc_macro_deps = [kwargs.get("prost_derive_dep", create_name_to_label("prost-derive"))],
        **{
            k: v
            for (k, v) in kwargs.items()
            if k in bazel_build_rule_common_attrs
        }  # Forward Bazel common args
    )
