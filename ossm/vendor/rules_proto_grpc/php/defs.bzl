"""php protobuf and grpc rules."""

load(":php_proto_compile.bzl", _php_proto_compile = "php_proto_compile")
load(":php_grpc_compile.bzl", _php_grpc_compile = "php_grpc_compile")

# Export php rules
php_proto_compile = _php_proto_compile
php_grpc_compile = _php_grpc_compile
