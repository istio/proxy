
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
        sha256 = "150c36fd4a5f5b133d77efc1ea2af2d4cbfe72e26dbc76d86131623091d7780d",
        strip_prefix = "envoy-openssl-422ad589053f1eb572dfe40408a8b2188aa7aa0f",
        url = "https://github.com/envoyproxy/envoy-openssl/archive/422ad589053f1eb572dfe40408a8b2188aa7aa0f.tar.gz",
        patch_args = ["-p1"],
        patches = [
            "@io_istio_proxy//ossm/patches:use-cmake-from-host.patch",
            ],
    )
