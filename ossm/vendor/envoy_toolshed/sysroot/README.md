# Sysroots

This directory contains both sysroot repository rules for downloading pre-built sysroots and build targets for creating new sysroots.

## Available Sysroots

The workflow builds sysroots with the following configurations:

### glibc 2.31 (Current)
- **Base:** Debian bullseye
- **Compatibility:** Modern Linux distributions
- **Kernel Headers:** 5.10+ (from bullseye-backports)
- **Variants:**
  - Base: `sysroot-glibc2.31-{arch}.tar.xz`
  - With libstdc++13: `sysroot-glibc2.31-libstdc++13-{arch}.tar.xz`

### glibc 2.28 (Older)
- **Base:** Debian buster
- **Compatibility:** Ubuntu 18.04, RHEL 8, and older distributions
- **Kernel Headers:** 5.10+ (from buster-backports)
- **Variants:**
  - Base: `sysroot-glibc2.28-{arch}.tar.xz`
  - With libstdc++13: `sysroot-glibc2.28-libstdc++13-{arch}.tar.xz`

## Architecture Support

Both glibc versions are built for:
- `amd64` (x86_64)
- `arm64` (aarch64)

## Kernel Headers

All sysroots include modern kernel headers (5.10+) from Debian backports, which provide:
- **openat2.h** support (Linux 5.6+)
- Modern syscall definitions
- Up-to-date kernel API headers

This ensures that even older glibc sysroots can compile code using modern kernel features.

## Usage in Bazel

### WORKSPACE Mode

To use these sysroots in your Bazel WORKSPACE:

```starlark
load("@toolshed//bazel/sysroot:sysroot.bzl", "setup_sysroots")

# Use default glibc 2.31 with libstdc++13
setup_sysroots()

# Or use older glibc 2.28 for broader compatibility
setup_sysroots(
    glibc_version = "2.28",
    stdcc_version = "13",
)

# Or use base sysroot without libstdc++
setup_sysroots(
    glibc_version = "2.31",
    stdcc_version = None,
)

# Or use multiple sysroot configurations with name prefixes
setup_sysroots(
    glibc_version = "2.31",
    stdcc_version = "13",
    name_prefix = "new",
)
setup_sysroots(
    glibc_version = "2.28",
    stdcc_version = "13",
    name_prefix = "old",
)
# This creates @new_sysroot_linux_amd64, @new_sysroot_linux_arm64,
# @old_sysroot_linux_amd64, and @old_sysroot_linux_arm64
```

### MODULE.bazel (bzlmod) Mode

To use these sysroots in your MODULE.bazel file:

```starlark
# Add envoy_toolshed as a dependency
bazel_dep(name = "envoy_toolshed", version = "0.3.12")

# Setup sysroots using the extension
sysroot_ext = use_extension("@envoy_toolshed//bazel/sysroot:extensions.bzl", "sysroot_extension")

# Use default glibc 2.31 with libstdc++13
sysroot_ext.setup()

# Or configure with specific options:
# sysroot_ext.setup(
#     glibc_version = "2.28",
#     stdcc_version = "13",
# )

# Or use base sysroot without libstdc++:
# sysroot_ext.setup(
#     glibc_version = "2.31",
#     stdcc_version = "",
# )

# Or use multiple sysroot configurations with name prefixes:
# sysroot_ext.setup(
#     glibc_version = "2.31",
#     stdcc_version = "13",
#     name_prefix = "new",
# )
# sysroot_ext.setup(
#     glibc_version = "2.28",
#     stdcc_version = "13",
#     name_prefix = "old",
# )

# Make the sysroots available to your module
use_repo(sysroot_ext, "sysroot_linux_amd64", "sysroot_linux_arm64")

# If using name prefixes, specify all repository names:
# use_repo(sysroot_ext, "new_sysroot_linux_amd64", "new_sysroot_linux_arm64",
#                       "old_sysroot_linux_amd64", "old_sysroot_linux_arm64")
```

### Configuration Validation

The setup will automatically validate your configuration:
- **Unsupported glibc versions** will fail with a clear error message
- **Incompatible combinations** (e.g., requesting a variant that doesn't exist) will fail
- **Missing hashes** (e.g., for unreleased configurations) will fail with a helpful message

All SHA256 hashes are centrally managed in `versions.bzl` for ease of maintenance.

## Building Sysroots with Bazel

You can build sysroots locally using Bazel. The builds are marked as `manual` so they won't be built by default.

### Available Build Targets

All sysroot build targets are in the `//bazel/sysroot` package:

```bash
# Build ALL sysroots at once (from the bazel/ directory):
cd bazel
bazel build //sysroot:sysroots

# Or build specific sysroot variants:
bazel build //sysroot:sysroot_glibc2.31_amd64
bazel build //sysroot:sysroot_glibc2.31_arm64
bazel build //sysroot:sysroot_glibc2.31_libstdcxx_amd64
bazel build //sysroot:sysroot_glibc2.31_libstdcxx_arm64
bazel build //sysroot:sysroot_glibc2.28_amd64
bazel build //sysroot:sysroot_glibc2.28_arm64
bazel build //sysroot:sysroot_glibc2.28_libstdcxx_amd64
bazel build //sysroot:sysroot_glibc2.28_libstdcxx_arm64
```

The built sysroots will be in `bazel-bin/sysroot/` directory.

### Direct Script Usage

You can also use the build script directly for custom configurations:

```bash
# Basic usage
cd bazel
bazel run //sysroot:build_sysroot -- \
  --arch amd64 \
  --glibc 2.31 \
  --debian bullseye \
  --variant base \
  --output /tmp/mysysroot

# Keep specific directories (e.g., keep bin and sbin for debugging)
bazel run //sysroot:build_sysroot -- \
  --arch amd64 \
  --glibc 2.31 \
  --debian bullseye \
  --keep-dirs bin,sbin

# Remove additional directories beyond the defaults
bazel run //sysroot:build_sysroot -- \
  --arch amd64 \
  --glibc 2.31 \
  --debian bullseye \
  --remove-dirs usr/games,usr/local

# Skip cleanup entirely (keep everything)
bazel run //sysroot:build_sysroot -- \
  --arch amd64 \
  --glibc 2.31 \
  --debian bullseye \
  --skip-cleanup
```

#### Cleanup Configuration

By default, the build script removes directories that are typically not needed for cross-compilation:
- System directories: `boot`, `dev`, `proc`, `sys`, `run`, etc.
- Binaries: `bin`, `sbin`, `usr/sbin`, and standalone utilities
- Documentation: `usr/share/doc`, `usr/share/man`, `usr/share/info`
- Other: `var`, `tmp`, `home`, `opt`, etc.

You can customize this behavior:
- **`--keep-dirs`**: Comma-separated list of directories to preserve from the default cleanup
- **`--remove-dirs`**: Comma-separated list of additional directories to remove
- **`--skip-cleanup`**: Skip the cleanup phase entirely

### Requirements

Building sysroots requires:
- `sudo` access (for debootstrap and chroot operations)
- `debootstrap` package installed
- Linux host (uses debootstrap and chroot)

**Note:** These builds are non-hermetic and require network access to download Debian packages.

## Release Process

Sysroots are automatically built and published when:
1. Changes are pushed to `main` that affect this directory or the workflow
2. A release is created with name starting with `bins`

The artifacts are uploaded to the release assets.

### Building for Release

To build all sysroot variants for a release, use the convenience target:

```bash
cd bazel
# Build all sysroots at once
bazel build //sysroot:sysroots
```

Or build specific glibc versions:

```bash
# Build all glibc 2.31 variants
bazel build //sysroot:sysroot_glibc2.31_amd64 //sysroot:sysroot_glibc2.31_arm64 \
            //sysroot:sysroot_glibc2.31_libstdcxx_amd64 //sysroot:sysroot_glibc2.31_libstdcxx_arm64

# Build all glibc 2.28 variants
bazel build //sysroot:sysroot_glibc2.28_amd64 //sysroot:sysroot_glibc2.28_arm64 \
            //sysroot:sysroot_glibc2.28_libstdcxx_amd64 //sysroot:sysroot_glibc2.28_libstdcxx_arm64
```
