
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
        sha256 = "ee32a07001997651b2d1483166b4f1eff4760fc6e62e8a0d49e986d3c36bf85a",
        strip_prefix = "envoy-openssl-9761bbf6e6fc357876d73932e4455e5442c68dbb",
        url = "https://github.com/envoyproxy/envoy-openssl/archive/9761bbf6e6fc357876d73932e4455e5442c68dbb.tar.gz",
        patch_args = ["-p1"],
        patches = [
            "@io_istio_proxy//ossm/patches:use-cmake-from-host.patch",
            ],
    )
