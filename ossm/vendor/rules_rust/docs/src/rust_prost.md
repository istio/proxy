<!-- Generated with Stardoc: http://skydoc.bazel.build -->

# rules_rust_prost

These build rules are used for building [protobufs][protobuf]/[gRPC][grpc] in [Rust][rust] with Bazel
using [Prost][prost] and [Tonic][tonic]

[rust]: http://www.rust-lang.org/
[protobuf]: https://developers.google.com/protocol-buffers/
[grpc]: https://grpc.io
[prost]: https://github.com/tokio-rs/prost
[tonic]: https://github.com/tokio-rs/tonic

## Rules

- [rust_prost_library](#rust_prost_library)
- [rust_prost_toolchain](#rust_prost_toolchain)

## Setup

```python
load("@rules_rust//proto/prost:repositories.bzl", "rust_prost_dependencies")

rust_prost_dependencies()

load("@rules_rust//proto/prost:transitive_repositories.bzl", "rust_prost_transitive_repositories")

rust_prost_transitive_repositories()
```

The `prost` and `tonic` rules do not specify a default toolchain in order to avoid mismatched
dependency issues. To setup the `prost` and `tonic` toolchain, please see the section
[Customizing `prost` and `tonic` Dependencies](#custom-prost-deps).

For additional information about Bazel toolchains, see [here](https://docs.bazel.build/versions/master/toolchains.html).

#### <a name="custom-prost-deps">Customizing `prost` and `tonic` Dependencies

These rules depend on the [`prost`] and [`tonic`] dependencies. To setup the necessary toolchain
for these rules, you must define a toolchain with the [`prost`], [`prost-types`], [`tonic`], [`protoc-gen-prost`], and [`protoc-gen-tonic`] crates as well as the [`protoc`] binary.

[`prost`]: https://crates.io/crates/prost
[`prost-types`]: https://crates.io/crates/prost-types
[`protoc-gen-prost`]: https://crates.io/crates/protoc-gen-prost
[`protoc-gen-tonic`]: https://crates.io/crates/protoc-gen-tonic
[`tonic`]: https://crates.io/crates/tonic
[`protoc`]: https://github.com/protocolbuffers/protobuf

To get access to these crates, you can use the [crate_universe](./crate_universe.md) repository
rules. For example:

```python
load("//crate_universe:defs.bzl", "crate", "crates_repository")

crates_repository(
    name = "crates_io",
    annotations = {
        "protoc-gen-prost": [crate.annotation(
            gen_binaries = ["protoc-gen-prost"],
            patch_args = [
                "-p1",
            ],
            patches = [
                # Note: You will need to use this patch until a version greater than `0.2.2` of
                # `protoc-gen-prost` is released.
                "@rules_rust//proto/prost/private/3rdparty/patches:protoc-gen-prost.patch",
            ],
        )],
        "protoc-gen-tonic": [crate.annotation(
            gen_binaries = ["protoc-gen-tonic"],
        )],
    },
    cargo_lockfile = "Cargo.Bazel.lock",
    mode = "remote",
    packages = {
        "prost": crate.spec(
            version = "0",
        ),
        "prost-types": crate.spec(
            version = "0",
        ),
        "protoc-gen-prost": crate.spec(
            version = "0",
        ),
        "protoc-gen-tonic": crate.spec(
            version = "0",
        ),
        "tonic": crate.spec(
            version = "0",
        ),
    },
    repository_name = "rules_rust_prost",
    tags = ["manual"],
)
```

You can then define a toolchain with the `rust_prost_toolchain` rule which uses the crates
defined above. For example:

```python
load("@rules_rust//proto/prost:defs.bzl", "rust_prost_toolchain")
load("@rules_rust//rust:defs.bzl", "rust_library_group")

rust_library_group(
    name = "prost_runtime",
    deps = [
        "@crates_io//:prost",
    ],
)

rust_library_group(
    name = "tonic_runtime",
    deps = [
        ":prost_runtime",
        "@crates_io//:tonic",
    ],
)

rust_prost_toolchain(
    name = "prost_toolchain_impl",
    prost_plugin = "@crates_io//:protoc-gen-prost__protoc-gen-prost",
    prost_runtime = ":prost_runtime",
    prost_types = "@crates_io//:prost-types",
    proto_compiler = "@com_google_protobuf//:protoc",
    tonic_plugin = "@crates_io//:protoc-gen-tonic__protoc-gen-tonic",
    tonic_runtime = ":tonic_runtime",
)

toolchain(
    name = "prost_toolchain",
    toolchain = "prost_toolchain_impl",
    toolchain_type = "@rules_rust//proto/prost:toolchain_type",
)
```

Lastly, you must register the toolchain in your `WORKSPACE` file. For example:

```python
register_toolchains("//toolchains:prost_toolchain")
```

---
---

<a id="rust_prost_library"></a>

## rust_prost_library

<pre>
rust_prost_library(<a href="#rust_prost_library-name">name</a>, <a href="#rust_prost_library-proto">proto</a>)
</pre>

A rule for generating a Rust library using Prost.

**ATTRIBUTES**


| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="rust_prost_library-name"></a>name |  A unique name for this target.   | <a href="https://bazel.build/concepts/labels#target-names">Name</a> | required |  |
| <a id="rust_prost_library-proto"></a>proto |  A `proto_library` target for which to generate Rust gencode.   | <a href="https://bazel.build/concepts/labels">Label</a> | required |  |


<a id="rust_prost_toolchain"></a>

## rust_prost_toolchain

<pre>
rust_prost_toolchain(<a href="#rust_prost_toolchain-name">name</a>, <a href="#rust_prost_toolchain-include_transitive_deps">include_transitive_deps</a>, <a href="#rust_prost_toolchain-prost_opts">prost_opts</a>, <a href="#rust_prost_toolchain-prost_plugin">prost_plugin</a>, <a href="#rust_prost_toolchain-prost_plugin_flag">prost_plugin_flag</a>,
                     <a href="#rust_prost_toolchain-prost_runtime">prost_runtime</a>, <a href="#rust_prost_toolchain-prost_types">prost_types</a>, <a href="#rust_prost_toolchain-proto_compiler">proto_compiler</a>, <a href="#rust_prost_toolchain-tonic_opts">tonic_opts</a>, <a href="#rust_prost_toolchain-tonic_plugin">tonic_plugin</a>,
                     <a href="#rust_prost_toolchain-tonic_plugin_flag">tonic_plugin_flag</a>, <a href="#rust_prost_toolchain-tonic_runtime">tonic_runtime</a>)
</pre>

Rust Prost toolchain rule.

**ATTRIBUTES**


| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="rust_prost_toolchain-name"></a>name |  A unique name for this target.   | <a href="https://bazel.build/concepts/labels#target-names">Name</a> | required |  |
| <a id="rust_prost_toolchain-include_transitive_deps"></a>include_transitive_deps |  Whether to include transitive dependencies. If set to True, all transitive dependencies will directly accessible by the dependent crate.   | Boolean | optional |  `False`  |
| <a id="rust_prost_toolchain-prost_opts"></a>prost_opts |  Additional options to add to Prost.   | List of strings | optional |  `[]`  |
| <a id="rust_prost_toolchain-prost_plugin"></a>prost_plugin |  Additional plugins to add to Prost.   | <a href="https://bazel.build/concepts/labels">Label</a> | required |  |
| <a id="rust_prost_toolchain-prost_plugin_flag"></a>prost_plugin_flag |  Prost plugin flag format. (e.g. `--plugin=protoc-gen-prost=%s`)   | String | optional |  `"--plugin=protoc-gen-prost=%s"`  |
| <a id="rust_prost_toolchain-prost_runtime"></a>prost_runtime |  The Prost runtime crates to use.   | <a href="https://bazel.build/concepts/labels">Label</a> | required |  |
| <a id="rust_prost_toolchain-prost_types"></a>prost_types |  The Prost types crates to use.   | <a href="https://bazel.build/concepts/labels">Label</a> | required |  |
| <a id="rust_prost_toolchain-proto_compiler"></a>proto_compiler |  The protoc compiler to use. Note that this attribute is deprecated - prefer to use --incompatible_enable_proto_toolchain_resolution.   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `None`  |
| <a id="rust_prost_toolchain-tonic_opts"></a>tonic_opts |  Additional options to add to Tonic.   | List of strings | optional |  `[]`  |
| <a id="rust_prost_toolchain-tonic_plugin"></a>tonic_plugin |  Additional plugins to add to Tonic.   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `None`  |
| <a id="rust_prost_toolchain-tonic_plugin_flag"></a>tonic_plugin_flag |  Tonic plugin flag format. (e.g. `--plugin=protoc-gen-tonic=%s`))   | String | optional |  `"--plugin=protoc-gen-tonic=%s"`  |
| <a id="rust_prost_toolchain-tonic_runtime"></a>tonic_runtime |  The Tonic runtime crates to use.   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `None`  |


<a id="rust_prost_transform"></a>

## rust_prost_transform

<pre>
rust_prost_transform(<a href="#rust_prost_transform-name">name</a>, <a href="#rust_prost_transform-deps">deps</a>, <a href="#rust_prost_transform-srcs">srcs</a>, <a href="#rust_prost_transform-prost_opts">prost_opts</a>, <a href="#rust_prost_transform-tonic_opts">tonic_opts</a>)
</pre>

A rule for transforming the outputs of `ProstGenProto` actions.

This rule is used by adding it to the `data` attribute of `proto_library` targets. E.g.
```python
load("@rules_proto//proto:defs.bzl", "proto_library")
load("@rules_rust_prost//:defs.bzl", "rust_prost_library", "rust_prost_transform")

rust_prost_transform(
    name = "a_transform",
    srcs = [
        "a_src.rs",
    ],
)

proto_library(
    name = "a_proto",
    srcs = [
        "a.proto",
    ],
    data = [
        ":transform",
    ],
)

rust_prost_library(
    name = "a_rs_proto",
    proto = ":a_proto",
)
```

The `rust_prost_library` will spawn an action on the `a_proto` target which consumes the
`a_transform` rule to provide a means of granularly modifying a proto library for `ProstGenProto`
actions with minimal impact to other consumers.

**ATTRIBUTES**


| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="rust_prost_transform-name"></a>name |  A unique name for this target.   | <a href="https://bazel.build/concepts/labels#target-names">Name</a> | required |  |
| <a id="rust_prost_transform-deps"></a>deps |  Additional dependencies to add to the compiled crate.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |
| <a id="rust_prost_transform-srcs"></a>srcs |  Additional source files to include in generated Prost source code.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |
| <a id="rust_prost_transform-prost_opts"></a>prost_opts |  Additional options to add to Prost.   | List of strings | optional |  `[]`  |
| <a id="rust_prost_transform-tonic_opts"></a>tonic_opts |  Additional options to add to Tonic.   | List of strings | optional |  `[]`  |


