"""# rules_rust_wasm_bindgen

Bazel rules for generating wasm modules for Javascript using [wasm-bindgen][wb].

## Rules

- [rust_wasm_bindgen](#rust_wasm_bindgen)
- [rust_wasm_bindgen_toolchain](#rust_wasm_bindgen_toolchain)

## Setup

To begin using the `wasm-bindgen` rules, users can load the necessary dependencies
in their workspace by adding the following to their `WORKSPACE.bazel` file.

```python
load("@rules_rust_wasm_bindgen//:repositories.bzl", "rust_wasm_bindgen_dependencies", "rust_wasm_bindgen_register_toolchains")

rust_wasm_bindgen_dependencies()

rust_wasm_bindgen_register_toolchains()
```

This should enable users to start using the [rust_wasm_bindgen](#rust_wasm_bindgen)
rule. However, it's common to want to control the version of `wasm-bindgen` in the
workspace instead of relying on the one provided by `rules_rust`. In this case, users
should avoid calling `rust_wasm_bindgen_register_toolchains` and instead use the
[rust_wasm_bindgen_toolchain](#rust_wasm_bindgen_toolchain) rule to define their own
toolchains to register in the workspace.

## Interfacing with Javascript rules

Rules for doing so can be found at [rules_js_rust_wasm_bindgen](https://github.com/UebelAndre/rules_js_rust_wasm_bindgen)


[wb]: https://github.com/rustwasm/wasm-bindgen
"""

load(
    "//:providers.bzl",
    _RustWasmBindgenInfo = "RustWasmBindgenInfo",
)
load(
    "//private:wasm_bindgen.bzl",
    _rust_wasm_bindgen = "rust_wasm_bindgen",
    _rust_wasm_bindgen_toolchain = "rust_wasm_bindgen_toolchain",
)
load(
    "//private:wasm_bindgen_test.bzl",
    _rust_wasm_bindgen_test = "rust_wasm_bindgen_test",
)

rust_wasm_bindgen = _rust_wasm_bindgen
rust_wasm_bindgen_toolchain = _rust_wasm_bindgen_toolchain
rust_wasm_bindgen_test = _rust_wasm_bindgen_test
RustWasmBindgenInfo = _RustWasmBindgenInfo
