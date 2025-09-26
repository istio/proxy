<!-- Generated with Stardoc: http://skydoc.bazel.build -->

Rust WASM-bindgen rules for interfacing with aspect-build/rules_js

<a id="js_rust_wasm_bindgen"></a>

## js_rust_wasm_bindgen

<pre>
js_rust_wasm_bindgen(<a href="#js_rust_wasm_bindgen-name">name</a>, <a href="#js_rust_wasm_bindgen-bindgen_flags">bindgen_flags</a>, <a href="#js_rust_wasm_bindgen-out_name">out_name</a>, <a href="#js_rust_wasm_bindgen-target">target</a>, <a href="#js_rust_wasm_bindgen-target_arch">target_arch</a>, <a href="#js_rust_wasm_bindgen-wasm_file">wasm_file</a>)
</pre>

Generates javascript and typescript bindings for a webassembly module using [wasm-bindgen][ws] that interface with [aspect-build/rules_js][abjs].

[ws]: https://rustwasm.github.io/docs/wasm-bindgen/
[abjs]: https://github.com/aspect-build/rules_js

**ATTRIBUTES**


| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="js_rust_wasm_bindgen-name"></a>name |  A unique name for this target.   | <a href="https://bazel.build/concepts/labels#target-names">Name</a> | required |  |
| <a id="js_rust_wasm_bindgen-bindgen_flags"></a>bindgen_flags |  Flags to pass directly to the wasm-bindgen executable. See https://github.com/rustwasm/wasm-bindgen/ for details.   | List of strings | optional |  `[]`  |
| <a id="js_rust_wasm_bindgen-out_name"></a>out_name |  Set a custom output filename (Without extension. Defaults to target name).   | String | optional |  `""`  |
| <a id="js_rust_wasm_bindgen-target"></a>target |  The type of output to generate. See https://rustwasm.github.io/wasm-bindgen/reference/deployment.html for details.   | String | optional |  `"bundler"`  |
| <a id="js_rust_wasm_bindgen-target_arch"></a>target_arch |  The target architecture to use for the wasm-bindgen command line option.   | String | optional |  `"wasm32"`  |
| <a id="js_rust_wasm_bindgen-wasm_file"></a>wasm_file |  The `.wasm` file or crate to generate bindings for.   | <a href="https://bazel.build/concepts/labels">Label</a> | required |  |


