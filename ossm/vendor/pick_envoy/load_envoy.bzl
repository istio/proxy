
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
        sha256 = "c61555684ff8c71e2cc66af8c5d3d3e72d216d7435e851fc731b209bdfe2d82f",
        strip_prefix = "envoy-openssl-8a307659694f7c0c83c8d9cde8002099fca55198",
        url = "https://github.com/envoyproxy/envoy-openssl/archive/8a307659694f7c0c83c8d9cde8002099fca55198.tar.gz",
        patch_args = ["-p1"],
        patches = [
            "@io_istio_proxy//ossm/patches:use-cmake-from-host.patch",
            ],
    )
