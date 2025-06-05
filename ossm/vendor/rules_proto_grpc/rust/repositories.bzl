"""Common dependencies for rules_proto_grpc Rust rules."""

load(
    "//:repositories.bzl",
    "rules_proto_grpc_repos",
    "rules_rust",
)

def rust_repos(**kwargs):  # buildifier: disable=function-docstring
    rules_proto_grpc_repos(**kwargs)
    rules_rust(**kwargs)
