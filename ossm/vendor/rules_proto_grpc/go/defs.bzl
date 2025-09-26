"""go protobuf and grpc rules."""

load(":go_proto_compile.bzl", _go_proto_compile = "go_proto_compile")
load(":go_grpc_compile.bzl", _go_grpc_compile = "go_grpc_compile")
load(":go_validate_compile.bzl", _go_validate_compile = "go_validate_compile")
load(":go_proto_library.bzl", _go_proto_library = "go_proto_library")
load(":go_grpc_library.bzl", _go_grpc_library = "go_grpc_library")
load(":go_validate_library.bzl", _go_validate_library = "go_validate_library")

# Export go rules
go_proto_compile = _go_proto_compile
go_grpc_compile = _go_grpc_compile
go_validate_compile = _go_validate_compile
go_proto_library = _go_proto_library
go_grpc_library = _go_grpc_library
go_validate_library = _go_validate_library
