"""Common dependencies for rules_proto_grpc JavaScript rules."""

load(
    "//:repositories.bzl",
    "build_bazel_rules_nodejs",
    "com_google_protobuf_javascript",
    "grpc_web_plugin_darwin_arm64",
    "grpc_web_plugin_darwin_x86_64",
    "grpc_web_plugin_linux",
    "grpc_web_plugin_windows",
    "rules_proto_grpc_repos",
)

def js_repos(**kwargs):  # buildifier: disable=function-docstring
    rules_proto_grpc_repos(**kwargs)
    build_bazel_rules_nodejs(**kwargs)
    com_google_protobuf_javascript(**kwargs)
    grpc_web_plugin_darwin_arm64(**kwargs)
    grpc_web_plugin_darwin_x86_64(**kwargs)
    grpc_web_plugin_linux(**kwargs)
    grpc_web_plugin_windows(**kwargs)
