
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
        sha256 = "3127ba87117e0dde01588d53bf4b9bb854ff8aa6170d2d4d4d6fd41aabf0b757",
        strip_prefix = "envoy-openssl-5c475d85597a9da38184235af3543bdd107f68ef",
        url = "https://github.com/envoyproxy/envoy-openssl/archive/5c475d85597a9da38184235af3543bdd107f68ef.tar.gz",
        patch_args = ["-p1"],
        patches = [
            "@io_istio_proxy//ossm/patches:use-cmake-from-host.patch",
            ],
    )
