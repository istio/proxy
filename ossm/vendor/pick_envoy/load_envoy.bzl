
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
        sha256 = "c9e77143f6846aabba341c6bc1c9d7dbfcfd908b9015b6688c96b2231766963b",
        strip_prefix = "envoy-openssl-ea6da0d2b986e76ecd250f6eb4be47517fcebc34",
        url = "https://github.com/envoyproxy/envoy-openssl/archive/ea6da0d2b986e76ecd250f6eb4be47517fcebc34.tar.gz",
        patch_args = ["-p1"],
        patches = [
            "@io_istio_proxy//ossm/patches:use-cmake-from-host.patch",
            ],
    )
