"""scala protobuf and grpc rules."""

load(":scala_proto_compile.bzl", _scala_proto_compile = "scala_proto_compile")
load(":scala_grpc_compile.bzl", _scala_grpc_compile = "scala_grpc_compile")
load(":scala_proto_library.bzl", _scala_proto_library = "scala_proto_library")
load(":scala_grpc_library.bzl", _scala_grpc_library = "scala_grpc_library")

# Export scala rules
scala_proto_compile = _scala_proto_compile
scala_grpc_compile = _scala_grpc_compile
scala_proto_library = _scala_proto_library
scala_grpc_library = _scala_grpc_library
