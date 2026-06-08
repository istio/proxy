"""BUILD file for sysroot package.

This package provides Bazel targets for building Debian sysroots for cross-compilation.

IMPORTANT: These are non-hermetic builds that:
  - Require sudo access
  - Use debootstrap and chroot
  - Download packages from Debian mirrors
  - Are marked as 'manual' so they don't build by default
  - Are marked as 'no-cache' and 'no-remote' to prevent caching

These targets are intended for specific purposes and take a while to build,
so they must be explicitly requested.
"""

load("@rules_shell//shell:sh_binary.bzl", "sh_binary")
load(":fs.bzl", "sysroots")

exports_files([
    "build_sysroot.sh",
    "config.yaml",
    "extensions.bzl",
    "fs.bzl",
    "sysroot.bzl",
])

ARCHES = ["amd64", "arm64"]
GLIBC = ["2.28", "2.31"]
STDCC = ["13", ""]

sh_binary(
    name = "build_sysroot",
    srcs = ["build_sysroot.sh"],
    visibility = ["//visibility:public"],
)

sysroots(
    arches = ARCHES,
    glibc = GLIBC,
    stdcc = STDCC,
)
