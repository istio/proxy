"""Common dependencies for rules_proto_grpc Ruby rules."""

load(
    "//:repositories.bzl",
    "bazelruby_rules_ruby",
    "rules_proto_grpc_repos",
)

def ruby_repos(**kwargs):  # buildifier: disable=function-docstring
    rules_proto_grpc_repos(**kwargs)
    bazelruby_rules_ruby(**kwargs)
