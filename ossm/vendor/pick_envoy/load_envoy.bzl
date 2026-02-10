
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
        sha256 = "5a84fa3646b130be7114f6942931a47b4863632501f376132ad14887046db5d6",
        strip_prefix = "envoy-openssl-a357a24f9ab83cd4fb51e60a7fbfb6f94cba87fb",
        url = "https://github.com/envoyproxy/envoy-openssl/archive/a357a24f9ab83cd4fb51e60a7fbfb6f94cba87fb.tar.gz",
        patch_args = ["-p1"],
        patches = [
            "@io_istio_proxy//ossm/patches:use-cmake-from-host.patch",
            ],
    )
