
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

OPENSSL_DISABLED_EXTENSIONS = [
            "envoy.tls.key_providers.cryptomb",
            "envoy.tls.key_providers.qat",
            "envoy.quic.deterministic_connection_id_generator",
            "envoy.quic.crypto_stream.server.quiche",
            "envoy.quic.proof_source.filter_chain",
        ]

def load_envoy():
    http_archive(
        name = "envoy",
        sha256 = "a0a8d79b60181a38afd0194ecfd6ae8cea7c9af85d70de2a67e91683cd31a9d9",
        strip_prefix = "envoy-openssl-4b967994baa429e1f26ae499db7392af0bb562e1",
        url = "https://github.com/envoyproxy/envoy-openssl/archive/4b967994baa429e1f26ae499db7392af0bb562e1.tar.gz",
        patch_args = ["-p1"],
        patches = [
            "@io_istio_proxy//ossm/patches:use-cmake-from-host.patch",
            ],
    )
