"""Repository rules for MSAN and TSAN libraries."""

load("//:versions.bzl", "VERSIONS")

def _sanitizer_libs_impl(ctx, sanitizer):
    """Implementation for sanitizer library repository rules."""
    arch = ctx.attr.arch or "x86_64"
    if arch == "x86_64":
        # Download from releases
        ctx.download_and_extract(
            url = "https://github.com/envoyproxy/toolshed/releases/download/bazel-bins-v{version}/{sanitizer}-llvm{llvm_version}-{arch}.tar.xz".format(
                arch = arch,
                version = ctx.attr.version,
                sanitizer = sanitizer,
                llvm_version = VERSIONS["llvm"],
            ),
            sha256 = ctx.attr.sha256,
            stripPrefix = "{}-libs-{}".format(sanitizer, arch),
        )
    else:
        fail("Compilation for non-x86_64 architectures not yet implemented")

    # Create BUILD file
    ctx.file("BUILD.bazel", """
package(default_visibility = ["//visibility:public"])

filegroup(
    name = "libs",
    srcs = glob(["lib/*.a"]),
)

cc_library(
    name = "{sanitizer}_libs",
    srcs = [":libs"],
    linkstatic = True,
    alwayslink = True,
)

# Also expose individual libraries
cc_library(
    name = "libcxx",
    srcs = ["lib/libc++.a"],
    linkstatic = True,
    alwayslink = True,
)

cc_library(
    name = "libcxxabi",
    srcs = ["lib/libc++abi.a"],
    linkstatic = True,
    alwayslink = True,
)
""".format(sanitizer = sanitizer))

def _msan_libs_impl(ctx):
    _sanitizer_libs_impl(ctx, "msan")

def _tsan_libs_impl(ctx):
    _sanitizer_libs_impl(ctx, "tsan")

msan_libs = repository_rule(
    implementation = _msan_libs_impl,
    attrs = {
        "version": attr.string(
            mandatory = True,
            doc = "Release version to download (e.g., 'bazel-bins-v1.0.0')",
        ),
        "sha256": attr.string(
            mandatory = True,
            doc = "SHA256 hash of the msan libs archive",
        ),
        "arch": attr.string(
            default = "x86_64",
            doc = "Architecture to target",
        ),
    },
    doc = "Downloads or builds MSAN libraries",
)

tsan_libs = repository_rule(
    implementation = _tsan_libs_impl,
    attrs = {
        "version": attr.string(
            mandatory = True,
            doc = "Release version to download (e.g., 'bazel-bins-v1.0.0')",
        ),
        "sha256": attr.string(
            mandatory = True,
            doc = "SHA256 hash of the tsan libs archive",
        ),
        "arch": attr.string(
            default = "x86_64",
            doc = "Architecture to target",
        ),
    },
    doc = "Downloads or builds TSAN libraries",
)

def setup_sanitizer_libs(
        msan_version = None,
        msan_sha256 = None,
        tsan_version = None,
        tsan_sha256 = None):
    """Setup function for WORKSPACE."""
    msan_libs(
        name = "msan_libs",
        version = msan_version or VERSIONS["bins_release"],
        sha256 = msan_sha256 or VERSIONS["msan_libs_sha256"],
    )

    tsan_libs(
        name = "tsan_libs",
        version = tsan_version or VERSIONS["bins_release"],
        sha256 = tsan_sha256 or VERSIONS["tsan_libs_sha256"],
    )
