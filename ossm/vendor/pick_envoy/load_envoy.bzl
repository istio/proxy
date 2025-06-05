
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
        sha256 = "5ea411a975a0d52c80a1227ace873071cccce71fe20e3923606377cd2e1f9028",
        strip_prefix = "envoy-openssl-2ecdd4cd60c947a09e3409ae097640440227c6b3",
        url = "https://github.com/dcillera/envoy-openssl/archive/2ecdd4cd60c947a09e3409ae097640440227c6b3.tar.gz",
        patch_args = ["-p1"],
        patches = [
            "@io_istio_proxy//ossm/patches:use-cmake-from-host.patch",
            ],
    )
