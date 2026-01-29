
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
        sha256 = "f4f3acf960a8adbde2af0bfbfc848d51341e1f1c37ba0c525a00d040aac66a9d",
        strip_prefix = "envoy-openssl-8809cfea7d8f98995be8c4f20e99ec3c82ad80a1",
        url = "https://github.com/envoyproxy/envoy-openssl/archive/8809cfea7d8f98995be8c4f20e99ec3c82ad80a1.tar.gz",
        patch_args = ["-p1"],
        patches = [
            "@io_istio_proxy//ossm/patches:use-cmake-from-host.patch",
            ],
    )
