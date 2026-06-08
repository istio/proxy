"""c protobuf and grpc rules."""

load(":c_proto_compile.bzl", _c_proto_compile = "c_proto_compile")
load(":c_proto_library.bzl", _c_proto_library = "c_proto_library")

# Export c rules
c_proto_compile = _c_proto_compile
c_proto_library = _c_proto_library
