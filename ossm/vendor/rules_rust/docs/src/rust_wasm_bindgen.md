<!-- Generated with Stardoc: http://skydoc.bazel.build -->

# rules_rust_wasm_bindgen

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

<a id="rust_wasm_bindgen"></a>

## rust_wasm_bindgen

<pre>
rust_wasm_bindgen(<a href="#rust_wasm_bindgen-name">name</a>, <a href="#rust_wasm_bindgen-bindgen_flags">bindgen_flags</a>, <a href="#rust_wasm_bindgen-out_name">out_name</a>, <a href="#rust_wasm_bindgen-target">target</a>, <a href="#rust_wasm_bindgen-target_arch">target_arch</a>, <a href="#rust_wasm_bindgen-wasm_file">wasm_file</a>)
</pre>

Generates javascript and typescript bindings for a webassembly module using [wasm-bindgen][ws].

[ws]: https://rustwasm.github.io/docs/wasm-bindgen/

An example of this rule in use can be seen at [@rules_rust//examples/wasm](../examples/wasm)

**ATTRIBUTES**


| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="rust_wasm_bindgen-name"></a>name |  A unique name for this target.   | <a href="https://bazel.build/concepts/labels#target-names">Name</a> | required |  |
| <a id="rust_wasm_bindgen-bindgen_flags"></a>bindgen_flags |  Flags to pass directly to the wasm-bindgen executable. See https://github.com/rustwasm/wasm-bindgen/ for details.   | List of strings | optional |  `[]`  |
| <a id="rust_wasm_bindgen-out_name"></a>out_name |  Set a custom output filename (Without extension. Defaults to target name).   | String | optional |  `""`  |
| <a id="rust_wasm_bindgen-target"></a>target |  The type of output to generate. See https://rustwasm.github.io/wasm-bindgen/reference/deployment.html for details.   | String | optional |  `"bundler"`  |
| <a id="rust_wasm_bindgen-target_arch"></a>target_arch |  The target architecture to use for the wasm-bindgen command line option.   | String | optional |  `"wasm32"`  |
| <a id="rust_wasm_bindgen-wasm_file"></a>wasm_file |  The `.wasm` file or crate to generate bindings for.   | <a href="https://bazel.build/concepts/labels">Label</a> | required |  |


<a id="rust_wasm_bindgen_toolchain"></a>

## rust_wasm_bindgen_toolchain

<pre>
rust_wasm_bindgen_toolchain(<a href="#rust_wasm_bindgen_toolchain-name">name</a>, <a href="#rust_wasm_bindgen_toolchain-bindgen">bindgen</a>)
</pre>

The tools required for the `rust_wasm_bindgen` rule.

In cases where users want to control or change the version of `wasm-bindgen` used by [rust_wasm_bindgen](#rust_wasm_bindgen),
a unique toolchain can be created as in the example below:

```python
load("@rules_rust_bindgen//:defs.bzl", "rust_bindgen_toolchain")

rust_bindgen_toolchain(
    bindgen = "//3rdparty/crates:wasm_bindgen_cli__bin",
)

toolchain(
    name = "wasm_bindgen_toolchain",
    toolchain = "wasm_bindgen_toolchain_impl",
    toolchain_type = "@rules_rust_wasm_bindgen//:toolchain_type",
)
```

Now that you have your own toolchain, you need to register it by
inserting the following statement in your `WORKSPACE` file:

```python
register_toolchains("//my/toolchains:wasm_bindgen_toolchain")
```

For additional information, see the [Bazel toolchains documentation][toolchains].

[toolchains]: https://docs.bazel.build/versions/master/toolchains.html

**ATTRIBUTES**


| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="rust_wasm_bindgen_toolchain-name"></a>name |  A unique name for this target.   | <a href="https://bazel.build/concepts/labels#target-names">Name</a> | required |  |
| <a id="rust_wasm_bindgen_toolchain-bindgen"></a>bindgen |  The label of a `wasm-bindgen-cli` executable.   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `None`  |


<a id="RustWasmBindgenInfo"></a>

## RustWasmBindgenInfo

<pre>
RustWasmBindgenInfo(<a href="#RustWasmBindgenInfo-js">js</a>, <a href="#RustWasmBindgenInfo-root">root</a>, <a href="#RustWasmBindgenInfo-snippets">snippets</a>, <a href="#RustWasmBindgenInfo-ts">ts</a>, <a href="#RustWasmBindgenInfo-wasm">wasm</a>)
</pre>

Info about wasm-bindgen outputs.

**FIELDS**


| Name  | Description |
| :------------- | :------------- |
| <a id="RustWasmBindgenInfo-js"></a>js |  Depset[File]: The Javascript files produced by `wasm-bindgen`.    |
| <a id="RustWasmBindgenInfo-root"></a>root |  str: The path to the root of the `wasm-bindgen --out-dir` directory.    |
| <a id="RustWasmBindgenInfo-snippets"></a>snippets |  File: The snippets directory produced by `wasm-bindgen`.    |
| <a id="RustWasmBindgenInfo-ts"></a>ts |  Depset[File]: The Typescript files produced by `wasm-bindgen`.    |
| <a id="RustWasmBindgenInfo-wasm"></a>wasm |  File: The `.wasm` file generated by `wasm-bindgen`.    |


