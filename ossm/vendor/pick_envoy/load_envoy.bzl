
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
        sha256 = "43d38bbed5f71bd813acc50225ca76ada7891118fd9961cee500d7c1dc429ae0",
        strip_prefix = "envoy-openssl-9e3e79bbe1a44e1f5491bd22c39704c22ff6b2d1",
        url = "https://github.com/envoyproxy/envoy-openssl/archive/9e3e79bbe1a44e1f5491bd22c39704c22ff6b2d1.tar.gz",
        patch_args = ["-p1"],
        patches = [
            "@io_istio_proxy//ossm/patches:use-cmake-from-host.patch",
            ],
    )
