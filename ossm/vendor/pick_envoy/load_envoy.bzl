
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
        sha256 = "5fc34bd9b9e1108c0c78817d1878d7a585fcf5d236aa4ce8b3073c8c7bb77dcf",
        strip_prefix = "envoy-openssl-5694cb38af44f921fad511177bb563f7c8548702",
        url = "https://github.com/envoyproxy/envoy-openssl/archive/5694cb38af44f921fad511177bb563f7c8548702.tar.gz",
        patch_args = ["-p1"],
        patches = [
            "@io_istio_proxy//ossm/patches:use-cmake-from-host.patch",
            ],
    )
