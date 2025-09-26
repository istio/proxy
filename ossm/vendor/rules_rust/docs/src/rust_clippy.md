<!-- Generated with Stardoc: http://skydoc.bazel.build -->
# Rust Clippy

* [rust_clippy](#rust_clippy)
* [rust_clippy_aspect](#rust_clippy_aspect)


## Overview


[Clippy][clippy] is a tool for catching common mistakes in Rust code and improving it. An
expansive list of lints and the justification can be found in their [documentation][docs].

[clippy]: https://github.com/rust-lang/rust-clippy#readme
[docs]: https://rust-lang.github.io/rust-clippy/


### Setup


Simply add the following to the `.bazelrc` file in the root of your workspace:

```text
build --aspects=@rules_rust//rust:defs.bzl%rust_clippy_aspect
build --output_groups=+clippy_checks
```

This will enable clippy on all [Rust targets](./defs.md).

Note that targets tagged with `no-clippy` will not perform clippy checks

To use a local clippy.toml, add the following flag to your `.bazelrc`. Note that due to
the upstream implementation of clippy, this file must be named either `.clippy.toml` or
`clippy.toml`. Using a custom config file requires Rust 1.34.0 or newer.

```text
build --@rules_rust//rust/settings:clippy.toml=//:clippy.toml
```

<a id="rust_clippy"></a>

## rust_clippy

<pre>
rust_clippy(<a href="#rust_clippy-name">name</a>, <a href="#rust_clippy-deps">deps</a>)
</pre>

Executes the clippy checker on a specific target.

Similar to `rust_clippy_aspect`, but allows specifying a list of dependencies within the build system.

For example, given the following example targets:

```python
load("@rules_rust//rust:defs.bzl", "rust_library", "rust_test")

rust_library(
    name = "hello_lib",
    srcs = ["src/lib.rs"],
)

rust_test(
    name = "greeting_test",
    srcs = ["tests/greeting.rs"],
    deps = [":hello_lib"],
)
```

Rust clippy can be set as a build target with the following:

```python
load("@rules_rust//rust:defs.bzl", "rust_clippy")

rust_clippy(
    name = "hello_library_clippy",
    testonly = True,
    deps = [
        ":hello_lib",
        ":greeting_test",
    ],
)
```

**ATTRIBUTES**


| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="rust_clippy-name"></a>name |  A unique name for this target.   | <a href="https://bazel.build/concepts/labels#target-names">Name</a> | required |  |
| <a id="rust_clippy-deps"></a>deps |  Rust targets to run clippy on.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |


<a id="rust_clippy_aspect"></a>

## rust_clippy_aspect

<pre>
rust_clippy_aspect(<a href="#rust_clippy_aspect-name">name</a>)
</pre>

Executes the clippy checker on specified targets.

This aspect applies to existing rust_library, rust_test, and rust_binary rules.

As an example, if the following is defined in `examples/hello_lib/BUILD.bazel`:

```python
load("@rules_rust//rust:defs.bzl", "rust_library", "rust_test")

rust_library(
    name = "hello_lib",
    srcs = ["src/lib.rs"],
)

rust_test(
    name = "greeting_test",
    srcs = ["tests/greeting.rs"],
    deps = [":hello_lib"],
)
```

Then the targets can be analyzed with clippy using the following command:

```output
$ bazel build --aspects=@rules_rust//rust:defs.bzl%rust_clippy_aspect               --output_groups=clippy_checks //hello_lib:all
```

**ASPECT ATTRIBUTES**



**ATTRIBUTES**


| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="rust_clippy_aspect-name"></a>name |  A unique name for this target.   | <a href="https://bazel.build/concepts/labels#target-names">Name</a> | required |  |


