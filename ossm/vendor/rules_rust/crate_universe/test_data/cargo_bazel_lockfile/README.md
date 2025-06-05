# Catalog of testdata files

## `multi_package-cargo-bazel-lock.json`

This was copied from `examples/crate_universe/multi_package/cargo-bazel-lock.json` at commit 53daac71c0755680c29f4c55ac5238cc063e8b31.

It's intended to be an example of a realistic and meaningfully complex lockfile.

If this file needs to change in order to make the tests using it pass, it's likely a breaking change has been made to either the lockfile format or the lockfile parser.

Ideally we should avoid breaking changes in the lockfile parser, so ideally future tests would _add new_ test lockfiles (and this one will remain static to show our API can still parse historic lockfiles), rather than modify existing lockfiles.

We don't formally make strong guarantees around API stability here, so it's ok to need to change this file if really needed, but if it's easy to preserve the compatibility we'd prefer to.
