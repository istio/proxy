# Wasm-bindgen patches

## [resolver](./resolver.patch)

This patch avoids the following issue when updating dependencies

```text
INFO: Running command line: bazel-bin/wasm_bindgen/3rdparty/crates_vendor.sh '--repin=full'

error: failed to parse manifest at `/tmp/.tmpQivIHn/Cargo.toml`

Caused by:
  cannot specify `resolver` field in both `[workspace]` and `[package]`

Error: Failed to update lockfile: exit status: 101
```
