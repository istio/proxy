# Sanitizer libraries

This directory contains build rules for creating hermetic LLVM sanitizer libraries (MSAN, TSAN) that can be used with Envoy.

## Building

To build the libraries locally:

```bash
cd bazel
bazel build //compile:cxx_msan
bazel build //compile:cxx_tsan
```

This will produce:
- `bazel-bin/compile/msan-libs-x86_64.tar.gz`
- `bazel-bin/compile/tsan-libs-x86_64.tar.gz`

## Updating prebuilt versions

The sanitizer libraries are automatically built and published to GitHub releases. To update:

1. **Make changes** to the build configuration and merge them to main

2. **Create a release** with the naming format `bazel-bins-v{version}`

3. **Wait for CI** to build and publish the binaries to the release

4. **Get SHA256 hashes** for the published artifacts:
   ```bash
   curl -L https://github.com/envoyproxy/toolshed/releases/download/bazel-bins-v1.0.0/msan-libs-x86_64.tar.gz | sha256sum
   curl -L https://github.com/envoyproxy/toolshed/releases/download/bazel-bins-v1.0.0/tsan-libs-x86_64.tar.gz | sha256sum
   ```

5. **Update versions.bzl** with the new release tag and SHA256 values:
   ```python
   "bins_release": "1.0.0",
   "msan_libs_sha256": "...",  # Add actual SHA256
   "tsan_libs_sha256": "...",  # Add actual SHA256
   ```
