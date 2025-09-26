
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
        sha256 = "b2ec3fbaec889e5ad13b6f2fda309b14df5c45bf4f55ea5a1fdfbe57f1d96a6b",
        strip_prefix = "envoy-openssl-9b4094fc871ede4d2f0f0bf5303841cab78f689e",
        url = "https://github.com/envoyproxy/envoy-openssl/archive/9b4094fc871ede4d2f0f0bf5303841cab78f689e.tar.gz",
        patch_args = ["-p1"],
        patches = [
            "@io_istio_proxy//ossm/patches:use-cmake-from-host.patch",
            ],
    )
