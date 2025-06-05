"""fsharp protobuf and grpc rules."""

load(":fsharp_proto_compile.bzl", _fsharp_proto_compile = "fsharp_proto_compile")
load(":fsharp_grpc_compile.bzl", _fsharp_grpc_compile = "fsharp_grpc_compile")
load(":fsharp_proto_library.bzl", _fsharp_proto_library = "fsharp_proto_library")
load(":fsharp_grpc_library.bzl", _fsharp_grpc_library = "fsharp_grpc_library")

# Export fsharp rules
fsharp_proto_compile = _fsharp_proto_compile
fsharp_grpc_compile = _fsharp_grpc_compile
fsharp_proto_library = _fsharp_proto_library
fsharp_grpc_library = _fsharp_grpc_library
