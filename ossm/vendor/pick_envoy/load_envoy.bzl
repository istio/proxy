
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
        sha256 = "e3863c6dbd6bd54ff89a0ed0360f6a47180955c5b0e5e72b607ace978690cd22",
        strip_prefix = "envoy-openssl-000bcb2213bbbc15fd984fec2c91212c9cabfc58",
        url = "https://github.com/envoyproxy/envoy-openssl/archive/000bcb2213bbbc15fd984fec2c91212c9cabfc58.tar.gz",
        patch_args = ["-p1"],
        patches = [
            "@io_istio_proxy//ossm/patches:use-cmake-from-host.patch",
            ],
    )
