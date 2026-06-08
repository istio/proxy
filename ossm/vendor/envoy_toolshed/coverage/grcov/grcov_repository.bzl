"""Repository rule for grcov."""

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_file")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")

GRCOV_VERSION = "0.10.0"
GRCOV_SHA256 = {
    "x86_64-unknown-linux-gnu": "3d09a8046219869429dc7dcc76709498fd695d358aa878bcffb7365720cb8f0d",
    "x86_64-unknown-linux-musl": "582d8600755fe66191fb78649730785e407fd704a95ac1b5e9b276d8ce65313f",
    "x86_64-apple-darwin": "25c2e4b4a66b7fe09c7e39a929ba78ed1c8e2fb044629640ae6b3ee7b45192ac",
    "aarch64-apple-darwin": "63de0db8e20f5faf4fd1fd3234021b118c257c93f863322b4d8d1ee188cf2c4d",
    "aarch64-unknown-linux-gnu": "7a846c5ce9ccec6922b726273c0f56f4e21b24adbcf3423bef9fd8cf24bd0e9c",
    "aarch64-unknown-linux-musl": "ab6d128188a21fe5e6d0f842166b807a0e9dcbcf054711c617c8662725090ba5",
}

def _get_platform_info(ctx):
    """Get platform information for selecting the correct grcov binary."""
    os_name = ctx.os.name
    arch = ctx.os.arch
    if os_name == "linux":
        # Use musl for better compatibility across Linux distributions
        if arch == "x86_64" or arch == "amd64":
            return "x86_64-unknown-linux-musl"
        elif arch == "aarch64":
            return "aarch64-unknown-linux-musl"
    elif os_name == "mac os x" or os_name == "darwin":
        if arch == "x86_64" or arch == "amd64":
            return "x86_64-apple-darwin"
        elif arch == "aarch64" or arch == "arm64":
            return "aarch64-apple-darwin"
    fail("Unsupported platform: {} {}".format(os_name, arch))

def _grcov_repo_impl(ctx):
    """Implementation of the grcov repository rule."""
    platform = _get_platform_info(ctx)
    url = "https://github.com/mozilla/grcov/releases/download/v{version}/grcov-{platform}.tar.bz2".format(
        version = GRCOV_VERSION,
        platform = platform,
    )
    ctx.download_and_extract(
        url = url,
        sha256 = GRCOV_SHA256[platform],
        stripPrefix = "",
    )
    ctx.file("BUILD.bazel", """
exports_files(["grcov"])

filegroup(
    name = "grcov_bin",
    srcs = ["grcov"],
    visibility = ["//visibility:public"],
)
""")

_grcov_repo = repository_rule(
    implementation = _grcov_repo_impl,
    attrs = {},
)

def grcov_repository():
    """Download grcov binary for the current platform."""
    maybe(
        _grcov_repo,
        name = "grcov",
    )
