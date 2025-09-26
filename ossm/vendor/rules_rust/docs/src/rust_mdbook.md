<!-- Generated with Stardoc: http://skydoc.bazel.build -->

# rules_rust_mdbook

Bazel rules for [mdBook](https://github.com/rust-lang/mdBook).

## Rules

- [mdbook](#mdbook)
- [mdbook_server](#mdbook_server)
- [mdbook_toolchain](#mdbook_toolchain)

## Setup

### bzlmod

```python
bazel_dep(name = "rules_rust_mdbook", version = "{SEE_RELEASE_NOTES}")
```

### WORKSPACE

```python
load("@rules_rust_mdbook//:repositories.bzl", "mdbook_register_toolchains", "rules_mdbook_dependencies")

rules_mdbook_dependencies()

mdbook_register_toolchains()

load("@rules_rust_mdbook//:repositories_transitive.bzl", "rules_mdbook_transitive_deps")

rules_mdbook_transitive_deps()
```

---
---

<a id="mdbook"></a>

## mdbook

<pre>
mdbook(<a href="#mdbook-name">name</a>, <a href="#mdbook-srcs">srcs</a>, <a href="#mdbook-book">book</a>, <a href="#mdbook-plugins">plugins</a>)
</pre>

Rules to create book from markdown files using `mdBook`.

**ATTRIBUTES**


| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="mdbook-name"></a>name |  A unique name for this target.   | <a href="https://bazel.build/concepts/labels#target-names">Name</a> | required |  |
| <a id="mdbook-srcs"></a>srcs |  All inputs to the book.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |
| <a id="mdbook-book"></a>book |  The `book.toml` file.   | <a href="https://bazel.build/concepts/labels">Label</a> | required |  |
| <a id="mdbook-plugins"></a>plugins |  Executables to inject into `PATH` for use in [preprocessor commands](https://rust-lang.github.io/mdBook/format/configuration/preprocessors.html#provide-your-own-command).   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |


<a id="mdbook_server"></a>

## mdbook_server

<pre>
mdbook_server(<a href="#mdbook_server-name">name</a>, <a href="#mdbook_server-book">book</a>, <a href="#mdbook_server-hostname">hostname</a>, <a href="#mdbook_server-port">port</a>)
</pre>

Spawn an mdbook server for a given `mdbook` target.

**ATTRIBUTES**


| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="mdbook_server-name"></a>name |  A unique name for this target.   | <a href="https://bazel.build/concepts/labels#target-names">Name</a> | required |  |
| <a id="mdbook_server-book"></a>book |  The `mdbook` target to serve.   | <a href="https://bazel.build/concepts/labels">Label</a> | required |  |
| <a id="mdbook_server-hostname"></a>hostname |  The default hostname to use (Can be overridden on the command line).   | String | optional |  `"localhost"`  |
| <a id="mdbook_server-port"></a>port |  The default port to use (Can be overridden on the command line).   | String | optional |  `"3000"`  |


<a id="mdbook_toolchain"></a>

## mdbook_toolchain

<pre>
mdbook_toolchain(<a href="#mdbook_toolchain-name">name</a>, <a href="#mdbook_toolchain-mdbook">mdbook</a>, <a href="#mdbook_toolchain-plugins">plugins</a>)
</pre>

A [mdBook](https://rust-lang.github.io/mdBook/) toolchain.

**ATTRIBUTES**


| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="mdbook_toolchain-name"></a>name |  A unique name for this target.   | <a href="https://bazel.build/concepts/labels#target-names">Name</a> | required |  |
| <a id="mdbook_toolchain-mdbook"></a>mdbook |  A `mdBook` binary.   | <a href="https://bazel.build/concepts/labels">Label</a> | required |  |
| <a id="mdbook_toolchain-plugins"></a>plugins |  Executables to inject into `PATH` for use in [preprocessor commands](https://rust-lang.github.io/mdBook/format/configuration/preprocessors.html#provide-your-own-command).   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |


