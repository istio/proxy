"""Common dependencies for rules_proto_grpc Swift rules."""

load(
    "//:repositories.bzl",
    "build_bazel_rules_swift",
    "com_github_apple_swift_log",
    "com_github_apple_swift_nio",
    "com_github_apple_swift_nio_extras",
    "com_github_apple_swift_nio_http2",
    "com_github_apple_swift_nio_ssl",
    "com_github_apple_swift_nio_transport_services",
    "com_github_grpc_grpc_swift",
    "rules_proto_grpc_repos",
)

def swift_repos(**kwargs):  # buildifier: disable=function-docstring
    rules_proto_grpc_repos(**kwargs)
    build_bazel_rules_swift(**kwargs)
    com_github_grpc_grpc_swift(**kwargs)
    com_github_apple_swift_log(**kwargs)
    com_github_apple_swift_nio(**kwargs)
    com_github_apple_swift_nio_extras(**kwargs)
    com_github_apple_swift_nio_http2(**kwargs)
    com_github_apple_swift_nio_ssl(**kwargs)
    com_github_apple_swift_nio_transport_services(**kwargs)
