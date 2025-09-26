"""Common dependencies for rules_proto_grpc Buf rules."""

load(
    "//:repositories.bzl",
    "protoc_gen_buf_breaking_darwin_arm64",
    "protoc_gen_buf_breaking_darwin_x86_64",
    "protoc_gen_buf_breaking_linux_x86_64",
    "protoc_gen_buf_lint_darwin_arm64",
    "protoc_gen_buf_lint_darwin_x86_64",
    "protoc_gen_buf_lint_linux_x86_64",
    "rules_proto_grpc_repos",
)

def buf_repos(**kwargs):  # buildifier: disable=function-docstring
    rules_proto_grpc_repos(**kwargs)
    protoc_gen_buf_breaking_darwin_arm64(**kwargs)
    protoc_gen_buf_breaking_darwin_x86_64(**kwargs)
    protoc_gen_buf_breaking_linux_x86_64(**kwargs)
    protoc_gen_buf_lint_darwin_arm64(**kwargs)
    protoc_gen_buf_lint_darwin_x86_64(**kwargs)
    protoc_gen_buf_lint_linux_x86_64(**kwargs)
