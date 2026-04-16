
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
        sha256 = "c81e5567f23823dd490133824222aeaaaf446886d4ea8426f2adf6fc474df1f6",
        strip_prefix = "envoy-openssl-3bc620d7bb79220b9f6ff74a4ed4b9bb9de426d6",
        url = "https://github.com/envoyproxy/envoy-openssl/archive/3bc620d7bb79220b9f6ff74a4ed4b9bb9de426d6.tar.gz",
        patch_args = ["-p1"],
        patches = [
            "@io_istio_proxy//ossm/patches:use-cmake-from-host.patch",
            ],
    )
