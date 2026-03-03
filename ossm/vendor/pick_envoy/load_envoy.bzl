
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
        sha256 = "e58e587ccc4fa1f02559904ccf71b466444e4820c83628aac78bd8402b2649cc",
        strip_prefix = "envoy-openssl-3dbf79e59ef15b92eaf5fa9da7c5075f08cda695",
        url = "https://github.com/envoyproxy/envoy-openssl/archive/3dbf79e59ef15b92eaf5fa9da7c5075f08cda695.tar.gz",
        patch_args = ["-p1"],
        patches = [
            "@io_istio_proxy//ossm/patches:use-cmake-from-host.patch",
            ],
    )
