
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
        sha256 = "87a0595955a449f418f28b7db75cecd9c22d79199f6c557bbeb1516c59c7e3dc",
        strip_prefix = "envoy-openssl-552de86eff1e77753f0ea5a1b22a89ebab53700b",
        url = "https://github.com/envoyproxy/envoy-openssl/archive/552de86eff1e77753f0ea5a1b22a89ebab53700b.tar.gz",
        patch_args = ["-p1"],
        patches = [
            "@io_istio_proxy//ossm/patches:use-cmake-from-host.patch",
            ],
    )
