"""ruby protobuf and grpc rules."""

load(":ruby_proto_compile.bzl", _ruby_proto_compile = "ruby_proto_compile")
load(":ruby_grpc_compile.bzl", _ruby_grpc_compile = "ruby_grpc_compile")
load(":ruby_proto_library.bzl", _ruby_proto_library = "ruby_proto_library")
load(":ruby_grpc_library.bzl", _ruby_grpc_library = "ruby_grpc_library")

# Export ruby rules
ruby_proto_compile = _ruby_proto_compile
ruby_grpc_compile = _ruby_grpc_compile
ruby_proto_library = _ruby_proto_library
ruby_grpc_library = _ruby_grpc_library
