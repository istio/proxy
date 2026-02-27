
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
        sha256 = "6df02fff54c8956d98a88ba4b4a455b72c85160c3a8497e0c10106625119456c",
        strip_prefix = "envoy-openssl-f328d5c9e6eb1fbb1b360dc25346fdcccfef32c6",
        url = "https://github.com/envoyproxy/envoy-openssl/archive/f328d5c9e6eb1fbb1b360dc25346fdcccfef32c6.tar.gz",
        patch_args = ["-p1"],
        patches = [
            "@io_istio_proxy//ossm/patches:use-cmake-from-host.patch",
            ],
    )
