"""Repository rule for glint."""

load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")
load("//:versions.bzl", "VERSIONS")

GLINT_VERSION = "0.1.0"

def _get_platform_info(rctx):
    """Get platform information for selecting the correct glint binary.

    Currently only Linux is supported as glint binaries are only built for Linux.
    """
    os_name = rctx.os.name
    arch = rctx.os.arch
    if os_name == "linux":
        if arch == "x86_64" or arch == "amd64":
            return "amd64"
        elif arch == "aarch64" or arch == "arm64":
            return "arm64"
    fail("Unsupported platform: {} {}. glint binaries are currently only available for Linux.".format(os_name, arch))

def _glint_repo_impl(rctx):
    """Implementation of the glint repository rule."""
    platform = _get_platform_info(rctx)
    url = "https://github.com/envoyproxy/toolshed/releases/download/bins-v{version}/glint-{glint_version}-{arch}".format(
        version = rctx.attr.bins_release_version,
        glint_version = GLINT_VERSION,
        arch = platform,
    )

    # Download the binary
    rctx.download(
        url = url,
        sha256 = VERSIONS["glint_sha256"].get(platform, ""),
        output = "glint",
        executable = True,
    )

    # Create a BUILD file that exports the binary
    rctx.file("BUILD.bazel", """
exports_files(["glint"])

filegroup(
    name = "glint_bin",
    srcs = ["glint"],
    visibility = ["//visibility:public"],
)

sh_binary(
    name = "glint",
    srcs = ["glint"],
    visibility = ["//visibility:public"],
)
""")

_glint_repo = repository_rule(
    implementation = _glint_repo_impl,
    attrs = {
        "bins_release_version": attr.string(
            doc = "Version of bins release to use",
            mandatory = True,
        ),
    },
)

def glint_repository(bins_release_version):
    """Download glint binary for the current platform.

    Args:
        bins_release_version: Version of the bins release (e.g., "0.1.21")
    """
    maybe(
        _glint_repo,
        name = "glint",
        bins_release_version = bins_release_version,
    )
