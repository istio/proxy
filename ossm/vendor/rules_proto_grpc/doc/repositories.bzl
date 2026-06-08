"""Common dependencies for rules_proto_grpc Documentation rules."""

load(
    "//:repositories.bzl",
    "protoc_gen_doc_darwin_arm64",
    "protoc_gen_doc_darwin_x86_64",
    "protoc_gen_doc_linux_x86_64",
    "protoc_gen_doc_windows_x86_64",
    "rules_proto_grpc_repos",
)

def doc_repos(**kwargs):  # buildifier: disable=function-docstring
    rules_proto_grpc_repos(**kwargs)
    protoc_gen_doc_darwin_arm64(**kwargs)
    protoc_gen_doc_darwin_x86_64(**kwargs)
    protoc_gen_doc_linux_x86_64(**kwargs)
    protoc_gen_doc_windows_x86_64(**kwargs)
