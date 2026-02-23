
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
        sha256 = "5d49a99eeabed22857ba2bd16ff7c63b476ddfb64570e9746aa8cf40a0b4857d",
        strip_prefix = "envoy-openssl-3bd94bd9d37b79972ce428580f66ed3c11098e17",
        url = "https://github.com/envoyproxy/envoy-openssl/archive/3bd94bd9d37b79972ce428580f66ed3c11098e17.tar.gz",
        patch_args = ["-p1"],
        patches = [
            "@io_istio_proxy//ossm/patches:use-cmake-from-host.patch",
            ],
    )
