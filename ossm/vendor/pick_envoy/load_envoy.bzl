
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
        sha256 = "6fde31377ee6aeedee20dacb352dd0e0f3382a498caca28b314fbcf7891e4911",
        strip_prefix = "envoy-openssl-08a71207339c1a24f95d27d9f17c5b14ca0dd24e",
        url = "https://github.com/envoyproxy/envoy-openssl/archive/08a71207339c1a24f95d27d9f17c5b14ca0dd24e.tar.gz",
        patch_args = ["-p1"],
        patches = [
            "@io_istio_proxy//ossm/patches:use-cmake-from-host.patch",
            ],
    )
