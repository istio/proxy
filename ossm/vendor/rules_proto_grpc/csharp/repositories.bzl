"""Common dependencies for rules_proto_grpc C# rules."""

load(
    "//:repositories.bzl",
    "io_bazel_rules_dotnet",
    "rules_proto_grpc_repos",
)

def csharp_repos(**kwargs):  # buildifier: disable=function-docstring
    rules_proto_grpc_repos(**kwargs)
    io_bazel_rules_dotnet(**kwargs)
