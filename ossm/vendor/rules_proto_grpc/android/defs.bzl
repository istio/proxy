"""android protobuf and grpc rules."""

load(":android_proto_compile.bzl", _android_proto_compile = "android_proto_compile")
load(":android_grpc_compile.bzl", _android_grpc_compile = "android_grpc_compile")
load(":android_proto_library.bzl", _android_proto_library = "android_proto_library")
load(":android_grpc_library.bzl", _android_grpc_library = "android_grpc_library")

# Export android rules
android_proto_compile = _android_proto_compile
android_grpc_compile = _android_grpc_compile
android_proto_library = _android_proto_library
android_grpc_library = _android_grpc_library
