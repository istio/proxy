"""Prost test dependencies"""

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")

def prost_test_deps():
    maybe(
        http_archive,
        name = "com_google_googleapis",
        urls = [
            "https://github.com/googleapis/googleapis/archive/18becb1d1426feb7399db144d7beeb3284f1ccb0.zip",
        ],
        strip_prefix = "googleapis-18becb1d1426feb7399db144d7beeb3284f1ccb0",
        sha256 = "b8c487191eb942361af905e40172644eab490190e717c3d09bf83e87f3994fff",
    )
