
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
        sha256 = "6d4ccfe35361e7f04e2e87288ea661004e1c43f8d1004c3d6d0b1279c0049abd",
        strip_prefix = "envoy-openssl-e0568a983e6236e042d9e887a82d9fb342fb6bda",
        url = "https://github.com/envoyproxy/envoy-openssl/archive/e0568a983e6236e042d9e887a82d9fb342fb6bda.tar.gz",
        patch_args = ["-p1"],
        patches = [
            "@io_istio_proxy//ossm/patches:use-cmake-from-host.patch",
            ],
    )
