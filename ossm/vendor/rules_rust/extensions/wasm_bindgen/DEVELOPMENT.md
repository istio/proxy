# Developing wasm_bindgen

## Updating [wasm-bindgen][wb]

Use the followign steps to update to new versions of [wasm-bindgen][wb].

1. Update `WASM_BINDGEN_VERSION` in `@rules_rust_wasm_bindgen//:repositories.bzl`
2. Update the sha256 value for the `rules_rust_wasm_bindgen_cli` repository defined in `@rules_rust_wasm_bindgen//:repositories.bzl` to match the artifact from the updated `WASM_BINDGEN_VERSION` value.
3. Regenerate dependencies by running `bazel run ///3rdparty:crates_vendor -- --repin` from the root of `rules_rust`.
4. Verify your changes by running `bazel test //wasm/...` from the `rules_rust/examples` directory.

[wb]: https://github.com/rustwasm/wasm-bindgen
