"""Common dependencies for rules_proto_grpc PHP rules."""

load(
    "//:repositories.bzl",
    "rules_proto_grpc_repos",
)

def php_repos(**kwargs):  # buildifier: disable=function-docstring
    rules_proto_grpc_repos(**kwargs)
