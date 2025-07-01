
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
        sha256 = "173a601b5940ccf0925fcd40986d106df5f89e05f27ef550f76079b64817fc54",
        strip_prefix = "envoy-openssl-5a776f186ff3803cd0bb07c45f22ff1635bd4317",
        url = "https://github.com/envoyproxy/envoy-openssl/archive/5a776f186ff3803cd0bb07c45f22ff1635bd4317.tar.gz",
        patch_args = ["-p1"],
        patches = [
            "@io_istio_proxy//ossm/patches:use-cmake-from-host.patch",
            ],
    )
