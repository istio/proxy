
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
        sha256 = "48938b313e77585f30d7e8a64ed7ed788520a17910538a0545f80b66e9e1084c",
        strip_prefix = "envoy-openssl-2a56b8afeaab950842ee8a1e05237364ca1fcf6c",
        url = "https://github.com/envoyproxy/envoy-openssl/archive/2a56b8afeaab950842ee8a1e05237364ca1fcf6c.tar.gz",
        patch_args = ["-p1"],
        patches = [
            "@io_istio_proxy//ossm/patches:use-cmake-from-host.patch",
            ],
    )
