"""Common dependencies for rules_proto_grpc F# rules."""

load(
    "//:repositories.bzl",
    "io_bazel_rules_dotnet",
    "rules_proto_grpc_repos",
)

def fsharp_repos(**kwargs):  # buildifier: disable=function-docstring
    rules_proto_grpc_repos(**kwargs)
    io_bazel_rules_dotnet(**kwargs)
