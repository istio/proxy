
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
        sha256 = "169dc09049d4625d2db45beabdb8484b6de69c4392dcd237ff721f99628bf7e2",
        strip_prefix = "envoy-openssl-f186af630e4fbad4ca3381c13475c51f2a1bd320",
        url = "https://github.com/envoyproxy/envoy-openssl/archive/f186af630e4fbad4ca3381c13475c51f2a1bd320.tar.gz",
        patch_args = ["-p1"],
        patches = [
            "@io_istio_proxy//ossm/patches:use-cmake-from-host.patch",
            ],
    )
