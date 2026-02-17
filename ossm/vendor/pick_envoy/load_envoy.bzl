
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
        sha256 = "0c03ede30e24686bad0ac6b4c842064f3a468676378b6dd14f8aec574eca8d50",
        strip_prefix = "envoy-openssl-40e3be5af48c79dbd9c0c19fd4dee633f9dea99d",
        url = "https://github.com/envoyproxy/envoy-openssl/archive/40e3be5af48c79dbd9c0c19fd4dee633f9dea99d.tar.gz",
        patch_args = ["-p1"],
        patches = [
            "@io_istio_proxy//ossm/patches:use-cmake-from-host.patch",
            ],
    )
