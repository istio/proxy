"""Definitions for loading transitive `@rules_rust_protobuf` dependencies"""

load("@bazel_features//:deps.bzl", "bazel_features_deps")
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")
load("@com_google_protobuf//:protobuf_deps.bzl", "protobuf_deps")
load("@rules_proto//proto:repositories.bzl", "rules_proto_dependencies")

def rust_proto_protobuf_transitive_repositories():
    """Load transitive dependencies of the `@rules_rust_protobuf` rules.

    This macro should be called immediately after the `rust_protobuf_dependencies` macro.
    """
    rules_proto_dependencies()

    bazel_features_deps()

    maybe(
        http_archive,
        name = "zlib",
        build_file = Label("//3rdparty:BUILD.zlib.bazel"),
        sha256 = "c3e5e9fdd5004dcb542feda5ee4f0ff0744628baf8ed2dd5d66f8ca1197cb1a1",
        strip_prefix = "zlib-1.2.11",
        urls = [
            "https://zlib.net/zlib-1.2.11.tar.gz",
            "https://storage.googleapis.com/mirror.tensorflow.org/zlib.net/zlib-1.2.11.tar.gz",
        ],
    )

    protobuf_deps()
