
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
        sha256 = "b79bbe7d187abec846e758b2dff038714f432686d5e0d846ddd74f6db46adb80",
        strip_prefix = "envoy-openssl-fb4778628a69c8d1513688d33524d220c75d0887",
        url = "https://github.com/envoyproxy/envoy-openssl/archive/fb4778628a69c8d1513688d33524d220c75d0887.tar.gz",
        patch_args = ["-p1"],
        patches = [
            "@io_istio_proxy//ossm/patches:use-cmake-from-host.patch",
            ],
    )
