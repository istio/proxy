# Bazel tar rule

General-purpose rule to create tar archives.

Unlike [pkg_tar from rules_pkg](https://github.com/bazelbuild/rules_pkg/blob/main/docs/latest.md#pkg_tar):

- It does not depend on any Python interpreter setup
- The "manifest" specification is a mature public API and uses a compact tabular format, fixing
  https://github.com/bazelbuild/rules_pkg/pull/238
- It doesn't rely custom program to produce the output, instead
  we rely on the well-known C++ program `tar(1)`.
  Specifically, we use the BSD variant of tar since it provides a means
  of controlling mtimes, uid, symlinks, etc.

We also provide full control for tar'ring binaries including their runfiles.

The `tar` binary is hermetic and fully statically-linked. See Design Notes below.

This rule was originally developed within bazel-lib.
Thanks to all the contributors who made it possible!

## Remote cache and RBE

The `Tar` mnemonic is used for actions that produce the tar file outputs.
Depending on the inputs provided, these can be very large.

If you use Remote Build Execution, the tar files should generally be written to the remote cache,
and never downloaded to the host machine running Bazel (sometimes called "Build without the Bytes").
Subsequent actions or tests which use them as inputs will have to stage the files on an executor.
Be careful to tune the size of the remote cache to handle the artifacts you store.

If you do NOT use Remote Build Execution, then you should avoid uploading the tar outputs to the remote cache.
It is commonly faster to re-run the `Tar` actions locally on the input files than to download a remote cache hit, especially if compression is not used.
Large tar files consume storage and network bandwidth of the cache, and can lead to overload.
Use a snippet like the following in `.bazelrc`:

```
# Avoid overloading the remote cache with tar outputs.
# See https://github.com/bazel-contrib/tar.bzl/blob/main/README.md#remote-cache-and-rbe
common --modify_execution_info=Tar=+no-remote-cache
```

## Examples

- Migrate from `pkg_tar`: https://github.com/bazel-contrib/tar.bzl/blob/main/examples/migrate-rules_pkg/BUILD
- Look through our test suite: https://github.com/bazel-contrib/tar.bzl/blob/main/tar/tests/BUILD

Note; this repository doesn't yet allow modes other than `create`, such as "append", "list", "update", "extract".
See https://registry.bazel.build/modules/rules_tar for this.

## API docs

- [tar](https://registry.bazel.build/docs/tar.bzl#tar-tar-bzl) Run BSD `tar(1)` to produce archives
- [mtree](https://registry.bazel.build/docs/tar.bzl#tar-mtree-bzl) The intermediate manifest format `mtree(8)` describing a tar operation

## Design notes

1. We start from libarchive, which is on the BCR: https://registry.bazel.build/modules/libarchive
1. You could choose to register a toolchain that builds from source, but most users want a pre-built tar binary: https://github.com/aspect-build/bsdtar-prebuilt
1. This repo defines toolchain types
  - `@tar.bzl//tar/toolchain:type` for exec platform to use in build actions
  - `@tar.bzl//tar/toolchain:target_type` for including a `tar` binary in the output tree
1. We register a sensible default toolchain using the pre-built binary: https://github.com/bazel-contrib/tar.bzl/blob/main/tar/private/toolchain/toolchain.bzl
1. Finally we provide a thin layer of starlark rule code for invoking `tar` within Bazel actions (aka. build steps)
