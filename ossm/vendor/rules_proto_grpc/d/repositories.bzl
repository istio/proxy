"""Common dependencies for rules_proto_grpc D rules."""

load(
    "//:repositories.bzl",
    "com_github_dcarp_protobuf_d",
    "io_bazel_rules_d",
    "rules_proto_grpc_repos",
)

def d_repos(**kwargs):  # buildifier: disable=function-docstring
    rules_proto_grpc_repos(**kwargs)
    com_github_dcarp_protobuf_d(**kwargs)
    io_bazel_rules_d(**kwargs)
