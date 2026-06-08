"""# rules_rust_bindgen

These rules are for using [Bindgen][bindgen] to generate [Rust][rust] bindings to C (and some C++) libraries.

[rust]: http://www.rust-lang.org/
[bindgen]: https://github.com/rust-lang/rust-bindgen

## Rules

- [rust_bindgen](#rust_bindgen)
- [rust_bindgen_library](#rust_bindgen_library)
- [rust_bindgen_toolchain](#rust_bindgen_toolchain)

## Setup

### Bzlmod

To use the Rust bindgen rules, add the following to your `MODULE.bazel` file:

```python
bazel_dep(name = "rules_rust_bindgen", version = "{SEE_RELEASE_NOTES}")
```

rules_rust_bindgen does not automatically register a bindgen toolchain.
You need to register either your own or the default toolchain by adding the following to your `MODULE.bazel` file:

```python
register_toolchains("@rules_rust_bindgen//:default_bindgen_toolchain")
```

The default toolchain builds libclang from source via the [llvm-project](https://registry.bazel.build/modules/llvm-project) bazel_dep.
[examples/bindgen_toolchain](https://github.com/bazelbuild/rules_rust/tree/main/examples/bindgen_toolchain) shows how to use a prebuilt libclang.

### Workspace

Or add the following if you're still using `WORKSPACE` to add the
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
[cra_uni]: https://bazelbuild.github.io/rules_rust/crate_universe_workspace.html

---
---
"""

load(
    "//private:bindgen.bzl",
    _rust_bindgen = "rust_bindgen",
    _rust_bindgen_library = "rust_bindgen_library",
    _rust_bindgen_toolchain = "rust_bindgen_toolchain",
)

rust_bindgen = _rust_bindgen
rust_bindgen_library = _rust_bindgen_library
rust_bindgen_toolchain = _rust_bindgen_toolchain
