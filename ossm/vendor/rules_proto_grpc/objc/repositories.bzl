"""Common dependencies for rules_proto_grpc Objective-C rules."""

load(
    "//:repositories.bzl",
    "rules_proto_grpc_repos",
)

def objc_repos(**kwargs):  # buildifier: disable=function-docstring
    rules_proto_grpc_repos(**kwargs)
