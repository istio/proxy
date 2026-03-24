
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
        sha256 = "7a894531441313af5630f3698378e176681e9119c3d372ea4fb9635190873d1e",
        strip_prefix = "envoy-openssl-24352e4e2a16a1eef07ae0047f4b8ae2f04006f3",
        url = "https://github.com/envoyproxy/envoy-openssl/archive/24352e4e2a16a1eef07ae0047f4b8ae2f04006f3.tar.gz",
        patch_args = ["-p1"],
        patches = [
            "@io_istio_proxy//ossm/patches:use-cmake-from-host.patch",
            ],
    )
