"""Generated definition of android_grpc_library."""

load("//android:android_grpc_compile.bzl", "android_grpc_compile")
load("//:defs.bzl", "bazel_build_rule_common_attrs", "proto_compile_attrs")
load("@build_bazel_rules_android//android:rules.bzl", "android_library")

def android_grpc_library(name, **kwargs):
    # Compile protos
    name_pb = name + "_pb"
    android_grpc_compile(
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
        deps = GRPC_DEPS + kwargs.get("deps", []),
        exports = GRPC_DEPS + kwargs.get("exports", []),
        **{
            k: v
            for (k, v) in kwargs.items()
            if k in bazel_build_rule_common_attrs
        }  # Forward Bazel common args
    )

GRPC_DEPS = [
    "@io_grpc_grpc_java//api",
    "@io_grpc_grpc_java//protobuf-lite",
    "@io_grpc_grpc_java//stub",
    "@io_grpc_grpc_java//stub:javax_annotation",
    "@com_google_code_findbugs_jsr305//jar",
    "@com_google_guava_guava//jar",
    "@com_google_protobuf//:protobuf_javalite",
    "@com_google_protobuf//:protobuf_java_util",
    Label("//android:well_known_protos"),  # Lite is missing gen_well_known_protos_java from protobuf, compile them manually
]
