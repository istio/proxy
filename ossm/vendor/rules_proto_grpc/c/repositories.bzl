"""Common dependencies for rules_proto_grpc C rules."""

load(
    "//:repositories.bzl",
    "rules_proto_grpc_repos",
)

def c_repos(**kwargs):  # buildifier: disable=function-docstring
    rules_proto_grpc_repos(**kwargs)
