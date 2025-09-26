<!-- Generated with Stardoc: http://skydoc.bazel.build -->

# rules_rust_bindgen

These rules are for using [Bindgen][bindgen] to generate [Rust][rust] bindings to C (and some C++) libraries.

[rust]: http://www.rust-lang.org/
[bindgen]: https://github.com/rust-lang/rust-bindgen

## Rules

- [rust_bindgen](#rust_bindgen)
- [rust_bindgen_library](#rust_bindgen_library)
- [rust_bindgen_toolchain](#rust_bindgen_toolchain)

## Setup

To use the Rust bindgen rules, add the following to your `WORKSPACE` file to add the
external repositories for the Rust bindgen toolchain (in addition to the [rust rules setup](https://bazelbuild.github.io/rules_rust/#setup)):

```python
load("@rules_rust_bindgen//:repositories.bzl", "rust_bindgen_dependencies", "rust_bindgen_register_toolchains")

rust_bindgen_dependencies()

rust_bindgen_register_toolchains()

load("@rules_rust_bindgen//:transitive_repositories.bzl", "rust_bindgen_transitive_dependencies")

rust_bindgen_transitive_dependencies()
```

Bindgen aims to be as hermetic as possible so will end up building `libclang` from [llvm-project][llvm_proj] from
source. If this is found to be undesirable then no Bindgen related calls should be added to your WORKSPACE and instead
users should define their own repositories using something akin to [crate_universe][cra_uni] and define their own
toolchains following the instructions for [rust_bindgen_toolchain](#rust_bindgen_toolchain).

[llvm_proj]: https://github.com/llvm/llvm-project
[cra_uni]: https://bazelbuild.github.io/rules_rust/crate_universe.html

---
---

<a id="rust_bindgen"></a>

## rust_bindgen

<pre>
rust_bindgen(<a href="#rust_bindgen-name">name</a>, <a href="#rust_bindgen-bindgen_flags">bindgen_flags</a>, <a href="#rust_bindgen-cc_lib">cc_lib</a>, <a href="#rust_bindgen-clang_flags">clang_flags</a>, <a href="#rust_bindgen-header">header</a>, <a href="#rust_bindgen-merge_cc_lib_objects_into_rlib">merge_cc_lib_objects_into_rlib</a>,
             <a href="#rust_bindgen-wrap_static_fns">wrap_static_fns</a>)
</pre>

Generates a rust source file from a cc_library and a header.

**ATTRIBUTES**


| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="rust_bindgen-name"></a>name |  A unique name for this target.   | <a href="https://bazel.build/concepts/labels#target-names">Name</a> | required |  |
| <a id="rust_bindgen-bindgen_flags"></a>bindgen_flags |  Flags to pass directly to the bindgen executable. See https://rust-lang.github.io/rust-bindgen/ for details.   | List of strings | optional |  `[]`  |
| <a id="rust_bindgen-cc_lib"></a>cc_lib |  The cc_library that contains the `.h` file. This is used to find the transitive includes.   | <a href="https://bazel.build/concepts/labels">Label</a> | required |  |
| <a id="rust_bindgen-clang_flags"></a>clang_flags |  Flags to pass directly to the clang executable.   | List of strings | optional |  `[]`  |
| <a id="rust_bindgen-header"></a>header |  The `.h` file to generate bindings for.   | <a href="https://bazel.build/concepts/labels">Label</a> | required |  |
| <a id="rust_bindgen-merge_cc_lib_objects_into_rlib"></a>merge_cc_lib_objects_into_rlib |  When True, objects from `cc_lib` will be copied into the `rlib` archive produced by the rust_library that depends on this `rust_bindgen` rule (using `BuildInfo` provider)   | Boolean | optional |  `True`  |
| <a id="rust_bindgen-wrap_static_fns"></a>wrap_static_fns |  Whether to create a separate .c file for static fns. Requires nightly toolchain, and a header that actually needs this feature (otherwise bindgen won't generate the file and Bazel complains).   | Boolean | optional |  `False`  |


<a id="rust_bindgen_toolchain"></a>

## rust_bindgen_toolchain

<pre>
rust_bindgen_toolchain(<a href="#rust_bindgen_toolchain-name">name</a>, <a href="#rust_bindgen_toolchain-bindgen">bindgen</a>, <a href="#rust_bindgen_toolchain-clang">clang</a>, <a href="#rust_bindgen_toolchain-default_rustfmt">default_rustfmt</a>, <a href="#rust_bindgen_toolchain-libclang">libclang</a>, <a href="#rust_bindgen_toolchain-libstdcxx">libstdcxx</a>)
</pre>

The tools required for the `rust_bindgen` rule.

This rule depends on the [`bindgen`](https://crates.io/crates/bindgen) binary crate, and it
in turn depends on both a clang binary and the clang library. To obtain these dependencies,
`rust_bindgen_dependencies` imports bindgen and its dependencies.

```python
load("@rules_rust_bindgen//:defs.bzl", "rust_bindgen_toolchain")

rust_bindgen_toolchain(
    name = "bindgen_toolchain_impl",
    bindgen = "//my/rust:bindgen",
    clang = "//my/clang:clang",
    libclang = "//my/clang:libclang.so",
    libstdcxx = "//my/cpp:libstdc++",
)

toolchain(
    name = "bindgen_toolchain",
    toolchain = "bindgen_toolchain_impl",
    toolchain_type = "@rules_rust_bindgen//:toolchain_type",
)
```

This toolchain will then need to be registered in the current `WORKSPACE`.
For additional information, see the [Bazel toolchains documentation](https://docs.bazel.build/versions/master/toolchains.html).

**ATTRIBUTES**


| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="rust_bindgen_toolchain-name"></a>name |  A unique name for this target.   | <a href="https://bazel.build/concepts/labels#target-names">Name</a> | required |  |
| <a id="rust_bindgen_toolchain-bindgen"></a>bindgen |  The label of a `bindgen` executable.   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `None`  |
| <a id="rust_bindgen_toolchain-clang"></a>clang |  The label of a `clang` executable.   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `None`  |
| <a id="rust_bindgen_toolchain-default_rustfmt"></a>default_rustfmt |  If set, `rust_bindgen` targets will always format generated sources with `rustfmt`.   | Boolean | optional |  `True`  |
| <a id="rust_bindgen_toolchain-libclang"></a>libclang |  A cc_library that provides bindgen's runtime dependency on libclang.   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `None`  |
| <a id="rust_bindgen_toolchain-libstdcxx"></a>libstdcxx |  A cc_library that satisfies libclang's libstdc++ dependency. This is used to make the execution of clang hermetic. If None, system libraries will be used instead.   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `None`  |


<a id="rust_bindgen_library"></a>

## rust_bindgen_library

<pre>
rust_bindgen_library(<a href="#rust_bindgen_library-name">name</a>, <a href="#rust_bindgen_library-header">header</a>, <a href="#rust_bindgen_library-cc_lib">cc_lib</a>, <a href="#rust_bindgen_library-bindgen_flags">bindgen_flags</a>, <a href="#rust_bindgen_library-bindgen_features">bindgen_features</a>, <a href="#rust_bindgen_library-clang_flags">clang_flags</a>,
                     <a href="#rust_bindgen_library-wrap_static_fns">wrap_static_fns</a>, <a href="#rust_bindgen_library-kwargs">kwargs</a>)
</pre>

Generates a rust source file for `header`, and builds a rust_library.

Arguments are the same as `rust_bindgen`, and `kwargs` are passed directly to rust_library.


**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="rust_bindgen_library-name"></a>name |  A unique name for this target.   |  none |
| <a id="rust_bindgen_library-header"></a>header |  The label of the .h file to generate bindings for.   |  none |
| <a id="rust_bindgen_library-cc_lib"></a>cc_lib |  The label of the cc_library that contains the .h file. This is used to find the transitive includes.   |  none |
| <a id="rust_bindgen_library-bindgen_flags"></a>bindgen_flags |  Flags to pass directly to the bindgen executable. See https://rust-lang.github.io/rust-bindgen/ for details.   |  `None` |
| <a id="rust_bindgen_library-bindgen_features"></a>bindgen_features |  The `features` attribute for the `rust_bindgen` target.   |  `None` |
| <a id="rust_bindgen_library-clang_flags"></a>clang_flags |  Flags to pass directly to the clang executable.   |  `None` |
| <a id="rust_bindgen_library-wrap_static_fns"></a>wrap_static_fns |  Whether to create a separate .c file for static fns. Requires nightly toolchain, and a header that actually needs this feature (otherwise bindgen won't generate the file and Bazel complains",   |  `False` |
| <a id="rust_bindgen_library-kwargs"></a>kwargs |  Arguments to forward to the underlying `rust_library` rule.   |  none |


