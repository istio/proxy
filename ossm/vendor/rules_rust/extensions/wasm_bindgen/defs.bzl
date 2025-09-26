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

While it's recommended for users to mantain their own , in the
`@rules_rust_wasm_bindgen` package there exists interface sub-packages for various
Javascript Bazel rules. E.g. `aspect_rules_js`. The rules defined there are a more
convenient way to use `rust_wasm_bindgen` with the associated javascript rules due
to the inclusion of additional providers. Each directory contains a `defs.bzl` file
that defines the different variants of `rust_wasm_bindgen`. (e.g. `js_rust_wasm_bindgen`
for the `rules_js` submodule).


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

rust_wasm_bindgen = _rust_wasm_bindgen
rust_wasm_bindgen_toolchain = _rust_wasm_bindgen_toolchain
RustWasmBindgenInfo = _RustWasmBindgenInfo
