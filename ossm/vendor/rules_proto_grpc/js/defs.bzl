"""js protobuf and grpc rules."""

load(":js_proto_compile.bzl", _js_proto_compile = "js_proto_compile")
load(":js_grpc_node_compile.bzl", _js_grpc_node_compile = "js_grpc_node_compile")
load(":js_grpc_web_compile.bzl", _js_grpc_web_compile = "js_grpc_web_compile")
load(":js_proto_library.bzl", _js_proto_library = "js_proto_library")
load(":js_grpc_node_library.bzl", _js_grpc_node_library = "js_grpc_node_library")
load(":js_grpc_web_library.bzl", _js_grpc_web_library = "js_grpc_web_library")

# Export js rules
js_proto_compile = _js_proto_compile
js_grpc_node_compile = _js_grpc_node_compile
js_grpc_web_compile = _js_grpc_web_compile
js_proto_library = _js_proto_library
js_grpc_node_library = _js_grpc_node_library
js_grpc_web_library = _js_grpc_web_library

# Aliases
nodejs_grpc_compile = _js_grpc_node_compile
nodejs_grpc_library = _js_grpc_node_library
nodejs_proto_compile = _js_proto_compile
nodejs_proto_library = _js_proto_library
