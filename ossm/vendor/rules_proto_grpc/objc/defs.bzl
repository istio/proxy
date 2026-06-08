"""objc protobuf and grpc rules."""

load(":objc_proto_compile.bzl", _objc_proto_compile = "objc_proto_compile")
load(":objc_grpc_compile.bzl", _objc_grpc_compile = "objc_grpc_compile")
load(":objc_proto_library.bzl", _objc_proto_library = "objc_proto_library")
load(":objc_grpc_library.bzl", _objc_grpc_library = "objc_grpc_library")

# Export objc rules
objc_proto_compile = _objc_proto_compile
objc_grpc_compile = _objc_grpc_compile
objc_proto_library = _objc_proto_library
objc_grpc_library = _objc_grpc_library
