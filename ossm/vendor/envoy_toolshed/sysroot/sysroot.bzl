"""Repository rules for sysroots."""

load("//:versions.bzl", "VERSIONS")

def _get_platform_arch(ctx):
    """Get the platform architecture for sysroot selection."""
    arch = ctx.os.arch
    if arch == "x86_64" or arch == "amd64":
        return "amd64"
    elif arch == "aarch64" or arch == "arm64":
        return "arm64"
    else:
        fail("Unsupported architecture: {}".format(arch))

def _sysroot_impl(ctx):
    """Implementation for sysroot repository rule."""
    arch = ctx.attr.arch or _get_platform_arch(ctx)
    ctx.download_and_extract(
        url = "https://github.com/envoyproxy/toolshed/releases/download/bazel-bins-v{version}/sysroot-glibc{glibc_version}-libstdc++{stdcc_version}-{arch}.tar.xz".format(
            version = ctx.attr.version,
            arch = arch,
            glibc_version = "2.31",
            stdcc_version = "13",
        ),
        sha256 = ctx.attr.sha256,
        stripPrefix = "",
    )
    ctx.file("BUILD.bazel", """
package(default_visibility = ["//visibility:public"])

filegroup(
    name = "sysroot",
    srcs = glob(
        ["**"],
        exclude = [
            "**/*:*",
            "**/*.pl",
        ],
    ),
)

filegroup(
    name = "headers",
    srcs = glob(
        ["usr/include/**"],
        exclude = ["**/*:*"],
    ),
)

filegroup(
    name = "libs",
    srcs = glob(
        [
            "usr/lib/**/*.a",
            "usr/lib/**/*.so*",
            "lib/**/*.a",
            "lib/**/*.so*",
        ],
        exclude = ["**/*:*"],
    ),
)

filegroup(
    name = "toolchain_sysroot",
    srcs = [":sysroot"],
)
""")

sysroot = repository_rule(
    implementation = _sysroot_impl,
    attrs = {
        "version": attr.string(
            mandatory = True,
            doc = "Release version to download (e.g., '1.0.0')",
        ),
        "sha256": attr.string(
            mandatory = True,
            doc = "SHA256 hash of the sysroot archive",
        ),
        "arch": attr.string(
            doc = "Architecture to download (amd64 or arm64). If not specified, uses host architecture",
        ),
    },
    doc = "Downloads sysroot for the specified or host architecture",
)

def setup_sysroots(
        version = None,
        amd64_sha256 = None,
        arm64_sha256 = None):
    """Setup function for WORKSPACE to configure sysroots.

    Args:
        version: Version of sysroot release to use
        amd64_sha256: SHA256 hash for amd64 sysroot
        arm64_sha256: SHA256 hash for arm64 sysroot
    """
    # AMD64 sysroot
    sysroot(
        name = "sysroot_linux_amd64",
        version = version or VERSIONS["bins_release"],
        sha256 = amd64_sha256 or VERSIONS["sysroot_amd64_sha256"],
        arch = "amd64",
    )
    # ARM64 sysroot
    sysroot(
        name = "sysroot_linux_arm64",
        version = version or VERSIONS["bins_release"],
        sha256 = arm64_sha256 or VERSIONS["sysroot_arm64_sha256"],
        arch = "arm64",
    )
