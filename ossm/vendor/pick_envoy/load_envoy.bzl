
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
        sha256 = "bccf7988faaf07571b996415c4cd340d33b414a5d065fcf835afee6fada991fc",
        strip_prefix = "envoy-openssl-57a90284750b517d00643435da9a29b3725d9065",
        url = "https://github.com/envoyproxy/envoy-openssl/archive/57a90284750b517d00643435da9a29b3725d9065.tar.gz",
        patch_args = ["-p1"],
        patches = [
            "@io_istio_proxy//ossm/patches:use-cmake-from-host.patch",
            ],
    )
