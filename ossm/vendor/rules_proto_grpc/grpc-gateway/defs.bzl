"""grpc-gateway protobuf and grpc rules."""

load(":gateway_grpc_compile.bzl", _gateway_grpc_compile = "gateway_grpc_compile")
load(":gateway_openapiv2_compile.bzl", _gateway_openapiv2_compile = "gateway_openapiv2_compile")
load(":gateway_grpc_library.bzl", _gateway_grpc_library = "gateway_grpc_library")

# Export grpc-gateway rules
gateway_grpc_compile = _gateway_grpc_compile
gateway_openapiv2_compile = _gateway_openapiv2_compile
gateway_grpc_library = _gateway_grpc_library
