"""swift protobuf and grpc rules."""

load(":swift_proto_compile.bzl", _swift_proto_compile = "swift_proto_compile")
load(":swift_grpc_compile.bzl", _swift_grpc_compile = "swift_grpc_compile")
load(":swift_proto_library.bzl", _swift_proto_library = "swift_proto_library")
load(":swift_grpc_library.bzl", _swift_grpc_library = "swift_grpc_library")

# Export swift rules
swift_proto_compile = _swift_proto_compile
swift_grpc_compile = _swift_grpc_compile
swift_proto_library = _swift_proto_library
swift_grpc_library = _swift_grpc_library
