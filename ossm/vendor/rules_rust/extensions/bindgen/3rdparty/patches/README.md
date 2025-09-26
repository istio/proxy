# Patches

All patches pair with the versions of the referenced repositories defined in `@rules_rust_bindgen//:repositories.bzl`.

## [llvm-project.incompatible_disallow_empty_glob](./llvm-project.incompatible_disallow_empty_glob.patch)

Uses of `glob` are updated to have `allow_empty = True` added so the llvm-project repo is compatible
with consumers building with [--incompatible_disallow_empty_glob](https://bazel.build/reference/command-line-reference#flag--incompatible_disallow_empty_glob).

## [llvm-raw.incompatible_disallow_empty_glob](./llvm-raw.incompatible_disallow_empty_glob.patch)

Similar to the `llvm-project` variant but is used outside of bzlmod.
