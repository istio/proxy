"""d protobuf and grpc rules."""

load(":d_proto_compile.bzl", _d_proto_compile = "d_proto_compile")
load(":d_proto_library.bzl", _d_proto_library = "d_proto_library")

# Export d rules
d_proto_compile = _d_proto_compile
d_proto_library = _d_proto_library
