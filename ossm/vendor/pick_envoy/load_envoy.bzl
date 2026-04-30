
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
        sha256 = "3c0a533db6b39d30a357199553920a0cc310161ae9034cd113bfd610641390b0",
        strip_prefix = "envoy-openssl-409f676b4b2b655cb905e61e1062798c65deb32a",
        url = "https://github.com/envoyproxy/envoy-openssl/archive/409f676b4b2b655cb905e61e1062798c65deb32a.tar.gz",
        patch_args = ["-p1"],
        patches = [
            "@io_istio_proxy//ossm/patches:use-cmake-from-host.patch",
            ],
    )
