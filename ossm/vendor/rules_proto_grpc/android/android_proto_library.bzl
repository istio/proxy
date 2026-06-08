"""Generated definition of android_proto_library."""

load("//android:android_proto_compile.bzl", "android_proto_compile")
load("//:defs.bzl", "bazel_build_rule_common_attrs", "proto_compile_attrs")
load("@build_bazel_rules_android//android:rules.bzl", "android_library")

def android_proto_library(name, **kwargs):
    # Compile protos
    name_pb = name + "_pb"
    android_proto_compile(
        name = name_pb,
        **{
            k: v
            for (k, v) in kwargs.items()
            if k in proto_compile_attrs.keys() or
               k in bazel_build_rule_common_attrs
        }  # Forward args
    )

    # Create android library
    android_library(
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
    "@com_google_protobuf//:protobuf_javalite",
    Label("//android:well_known_protos"),  # Lite is missing gen_well_known_protos_java from protobuf, compile them manually
]
