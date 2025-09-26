"""java protobuf and grpc rules."""

load(":java_proto_compile.bzl", _java_proto_compile = "java_proto_compile")
load(":java_grpc_compile.bzl", _java_grpc_compile = "java_grpc_compile")
load(":java_proto_library.bzl", _java_proto_library = "java_proto_library")
load(":java_grpc_library.bzl", _java_grpc_library = "java_grpc_library")

# Export java rules
java_proto_compile = _java_proto_compile
java_grpc_compile = _java_grpc_compile
java_proto_library = _java_proto_library
java_grpc_library = _java_grpc_library
