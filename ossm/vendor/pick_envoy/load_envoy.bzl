
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
        sha256 = "20a43cb88663e0c90496ac21bde04caf2aef5466f5d8292a2fbddf7e8e4d95bd",
        strip_prefix = "envoy-openssl-f8b640b7457111ab770a725cd221a69015e7ea34",
        url = "https://github.com/envoyproxy/envoy-openssl/archive/f8b640b7457111ab770a725cd221a69015e7ea34.tar.gz",
        patch_args = ["-p1"],
        patches = [
            "@io_istio_proxy//ossm/patches:use-cmake-from-host.patch",
            ],
    )
