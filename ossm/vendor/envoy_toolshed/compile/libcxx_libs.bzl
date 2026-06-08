"""Repository rules for prebuilt libcxx bundles for cross-compilation."""

load("//:versions.bzl", "VERSIONS")

def _libcxx_libs_impl(ctx):
    """Implementation for libcxx libs repository rule."""
    arch = ctx.attr.arch
    ctx.download_and_extract(
        url = "https://github.com/envoyproxy/toolshed/releases/download/bins-v{version}/libcxx-llvm{llvm_version}-{arch}.tar.xz".format(
            arch = arch,
            version = ctx.attr.version,
            llvm_version = VERSIONS["llvm"],
        ),
        sha256 = ctx.attr.sha256,
    )

    # Create BUILD file
    ctx.file("BUILD.bazel", """
package(default_visibility = ["//visibility:public"])

filegroup(
    name = "libcxx_libs_{arch}",
    srcs = glob(["include/**", "lib/**"]),
)

filegroup(
    name = "headers",
    srcs = glob(["include/**"]),
)

filegroup(
    name = "libs",
    srcs = glob(["lib/**"]),
)

""".format(arch = arch))

libcxx_libs = repository_rule(
    implementation = _libcxx_libs_impl,
    attrs = {
        "version": attr.string(
            mandatory = True,
            doc = "Release version to download (e.g., '0.1.46')",
        ),
        "sha256": attr.string(
            mandatory = True,
            doc = "SHA256 hash of the libcxx libs archive",
        ),
        "arch": attr.string(
            mandatory = True,
            doc = "Architecture to target (aarch64 or x86_64)",
            values = ["aarch64", "x86_64"],
        ),
    },
    doc = "Downloads prebuilt libcxx bundles for cross-compilation with toolchains_llvm",
)

def setup_libcxx_libs(
        aarch64_version = None,
        aarch64_sha256 = None,
        x86_64_version = None,
        x86_64_sha256 = None):
    """Setup function for WORKSPACE.

    Creates @libcxx_libs_aarch64 and @libcxx_libs_x86_64 repositories.
    """
    libcxx_libs(
        name = "libcxx_libs_aarch64",
        version = aarch64_version or VERSIONS["bins_release"],
        sha256 = aarch64_sha256 or VERSIONS["libcxx_libs_sha256"]["aarch64"],
        arch = "aarch64",
    )

    libcxx_libs(
        name = "libcxx_libs_x86_64",
        version = x86_64_version or VERSIONS["bins_release"],
        sha256 = x86_64_sha256 or VERSIONS["libcxx_libs_sha256"]["x86_64"],
        arch = "x86_64",
    )
