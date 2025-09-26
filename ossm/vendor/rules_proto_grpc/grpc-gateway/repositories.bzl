"""Common dependencies for rules_proto_grpc grpc-gateway rules."""

load(
    "//:repositories.bzl",
    "com_github_grpc_ecosystem_grpc_gateway_v2",
)
load("//go:repositories.bzl", "go_repos")

def gateway_repos(**kwargs):  # buildifier: disable=function-docstring
    go_repos(**kwargs)
    com_github_grpc_ecosystem_grpc_gateway_v2(**kwargs)
