<!-- Generated with Stardoc: http://skydoc.bazel.build -->
# Rust Proto

* [rust_prost_library](#rust_prost_library)
* [rust_prost_toolchain](#rust_prost_toolchain)
* [rust_prost_dependencies](#rust_prost_dependencies)
* [rust_prost_transitive_repositories](#rust_prost_transitive_repositories)
* [rust_proto_library](#rust_proto_library)
* [rust_grpc_library](#rust_grpc_library)
* [rust_proto_protobuf_toolchain](#rust_proto_protobuf_toolchain)
* [rust_proto_protobuf_dependencies](#rust_proto_protobuf_dependencies)
* [rust_proto_protobuf_register_toolchains](#rust_proto_protobuf_register_toolchains)
* [rust_proto_protobuf_transitive_repositories](#rust_proto_protobuf_transitive_repositories)


## Overview
These build rules are used for building [protobufs][protobuf]/[gRPC][grpc] in [Rust][rust] with Bazel.

There are two rule sets. The first ruleset defines the `rust_prost_library` which generates Rust code
using the [`prost`] and [`tonic`] dependencies. The second ruleset defines the `rust_proto_library` and
`rust_grpc_library` rules which generate Rust code using the [`rust-protobuf`] dependencies.

[rust]: http://www.rust-lang.org/
[protobuf]: https://developers.google.com/protocol-buffers/
[grpc]: https://grpc.io
[`rust-protobuf`]: https://github.com/stepancheg/rust-protobuf/

See the [protobuf example](../examples/proto) for a more complete example of use.

### Prost Setup

```python
load("@rules_rust_prost//:repositories.bzl", "rust_prost_dependencies")

rust_prost_dependencies()

load("@rules_rust_prost//:transitive_repositories.bzl", "rust_prost_transitive_repositories")

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
                "@rules_rust_prost///private/3rdparty/patches:protoc-gen-prost.patch",
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
load("@rules_rust_prost//:defs.bzl", "rust_prost_toolchain")
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
    toolchain_type = "@rules_rust_prost//:toolchain_type",
)
```

Lastly, you must register the toolchain in your `WORKSPACE` file. For example:

```python
register_toolchains("//toolchains:prost_toolchain")
```

## Rust-Protobuf Setup

To use the Rust proto rules, add the following to your `WORKSPACE` file to add the
external repositories for the Rust proto toolchain (in addition to the [rust rules setup](..)):

```python
load("@rules_rust_protobuf//:repositories.bzl", "rust_proto_protobuf_dependencies", "rust_proto_protobuf_register_toolchains")

rust_proto_protobuf_dependencies()

rust_proto_protobuf_register_toolchains()

load("@rules_rust_protobuf//:transitive_repositories.bzl", "rust_proto_protobuf_transitive_repositories")

rust_proto_protobuf_transitive_repositories()
```

This will load the required dependencies for the [`rust-protobuf`] rules. It will also
register a default toolchain for the `rust_proto_library` and `rust_grpc_library` rules.

To customize the `rust_proto_library` and `rust_grpc_library` toolchain, please see the section
[Customizing `rust-protobuf` Dependencies](#custom-proto-deps).

For additional information about Bazel toolchains, see [here](https://docs.bazel.build/versions/master/toolchains.html).

#### <a name="custom-proto-deps">Customizing `rust-protobuf` Dependencies

These rules depend on the [`protobuf`](https://crates.io/crates/protobuf) and
the [`grpc`](https://crates.io/crates/grpc) crates in addition to the [protobuf
compiler](https://github.com/google/protobuf). To obtain these crates,
`rust_proto_repositories` imports the given crates using BUILD files generated with
[crate_universe](./crate_universe.md).

If you want to either change the protobuf and gRPC rust compilers, or to
simply use [crate_universe](./crate_universe.md) in a more
complex scenario (with more dependencies), you must redefine those
dependencies.

To do this, once you've imported the needed dependencies (see our
[@rules_rust_protobuf///3rdparty/BUILD.bazel](https://github.com/bazelbuild/rules_rust/blob/main/proto/protobuf/3rdparty/BUILD.bazel)
file to see the default dependencies), you need to create your own toolchain.
To do so you can create a BUILD file with your toolchain definition, for example:

```python
load("@rules_rust//proto:toolchain.bzl", "rust_proto_toolchain")

rust_proto_toolchain(
    name = "proto-toolchain-impl",
    # Path to the protobuf compiler.
    protoc = "@com_google_protobuf//:protoc",
    # Protobuf compiler plugin to generate rust gRPC stubs.
    grpc_plugin = "//3rdparty/crates:cargo_bin_protoc_gen_rust_grpc",
    # Protobuf compiler plugin to generate rust protobuf stubs.
    proto_plugin = "//3rdparty/crates:cargo_bin_protoc_gen_rust",
)

toolchain(
    name = "proto-toolchain",
    toolchain = ":proto-toolchain-impl",
    toolchain_type = "@rules_rust_protobuf//:toolchain_type",
)
```

Now that you have your own toolchain, you need to register it by
inserting the following statement in your `WORKSPACE` file:

```python
register_toolchains("//my/toolchains:proto-toolchain")
```

Finally, you might want to set the `rust_deps` attribute in
`rust_proto_library` and `rust_grpc_library` to change the compile-time
dependencies:

```python
rust_proto_library(
    ...
    rust_deps = ["//3rdparty/crates:protobuf"],
    ...
)

rust_grpc_library(
    ...
    rust_deps = [
        "//3rdparty/crates:protobuf",
        "//3rdparty/crates:grpc",
        "//3rdparty/crates:tls_api",
        "//3rdparty/crates:tls_api_stub",
    ],
    ...
)
```

__Note__: Ideally, we would inject those dependencies from the toolchain,
but due to [bazelbuild/bazel#6889](https://github.com/bazelbuild/bazel/issues/6889)
all dependencies added via the toolchain ends-up being in the wrong
configuration.

---
---



<a id="rust_grpc_library"></a>

## rust_grpc_library

<pre>
rust_grpc_library(<a href="#rust_grpc_library-name">name</a>, <a href="#rust_grpc_library-deps">deps</a>, <a href="#rust_grpc_library-crate_name">crate_name</a>, <a href="#rust_grpc_library-rust_deps">rust_deps</a>, <a href="#rust_grpc_library-rustc_flags">rustc_flags</a>)
</pre>

Builds a Rust library crate from a set of `proto_library`s suitable for gRPC.

Example:

```python
load("@rules_rust_protobuf//:defs.bzl", "rust_grpc_library")

proto_library(
    name = "my_proto",
    srcs = ["my.proto"]
)

rust_grpc_library(
    name = "rust",
    deps = [":my_proto"],
)

rust_binary(
    name = "my_service",
    srcs = ["my_service.rs"],
    deps = [":rust"],
)
```

**ATTRIBUTES**


| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="rust_grpc_library-name"></a>name |  A unique name for this target.   | <a href="https://bazel.build/concepts/labels#target-names">Name</a> | required |  |
| <a id="rust_grpc_library-deps"></a>deps |  List of proto_library dependencies that will be built. One crate for each proto_library will be created with the corresponding gRPC stubs.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | required |  |
| <a id="rust_grpc_library-crate_name"></a>crate_name |  Crate name to use for this target.<br><br>This must be a valid Rust identifier, i.e. it may contain only alphanumeric characters and underscores. Defaults to the target name, with any hyphens replaced by underscores.   | String | optional |  `""`  |
| <a id="rust_grpc_library-rust_deps"></a>rust_deps |  The crates the generated library depends on.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |
| <a id="rust_grpc_library-rustc_flags"></a>rustc_flags |  List of compiler flags passed to `rustc`.<br><br>These strings are subject to Make variable expansion for predefined source/output path variables like `$location`, `$execpath`, and `$rootpath`. This expansion is useful if you wish to pass a generated file of arguments to rustc: `@$(location //package:target)`.   | List of strings | optional |  `[]`  |


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


<a id="rust_proto_library"></a>

## rust_proto_library

<pre>
rust_proto_library(<a href="#rust_proto_library-name">name</a>, <a href="#rust_proto_library-deps">deps</a>, <a href="#rust_proto_library-crate_name">crate_name</a>, <a href="#rust_proto_library-rust_deps">rust_deps</a>, <a href="#rust_proto_library-rustc_flags">rustc_flags</a>)
</pre>

Builds a Rust library crate from a set of `proto_library`s.

Example:

```python
load("@rules_rust_protobuf//:defs.bzl", "rust_proto_library")

proto_library(
    name = "my_proto",
    srcs = ["my.proto"]
)

rust_proto_library(
    name = "rust",
    deps = [":my_proto"],
)

rust_binary(
    name = "my_proto_binary",
    srcs = ["my_proto_binary.rs"],
    deps = [":rust"],
)
```

**ATTRIBUTES**


| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="rust_proto_library-name"></a>name |  A unique name for this target.   | <a href="https://bazel.build/concepts/labels#target-names">Name</a> | required |  |
| <a id="rust_proto_library-deps"></a>deps |  List of proto_library dependencies that will be built. One crate for each proto_library will be created with the corresponding stubs.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | required |  |
| <a id="rust_proto_library-crate_name"></a>crate_name |  Crate name to use for this target.<br><br>This must be a valid Rust identifier, i.e. it may contain only alphanumeric characters and underscores. Defaults to the target name, with any hyphens replaced by underscores.   | String | optional |  `""`  |
| <a id="rust_proto_library-rust_deps"></a>rust_deps |  The crates the generated library depends on.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |
| <a id="rust_proto_library-rustc_flags"></a>rustc_flags |  List of compiler flags passed to `rustc`.<br><br>These strings are subject to Make variable expansion for predefined source/output path variables like `$location`, `$execpath`, and `$rootpath`. This expansion is useful if you wish to pass a generated file of arguments to rustc: `@$(location //package:target)`.   | List of strings | optional |  `[]`  |


<a id="rust_prost_dependencies"></a>

## rust_prost_dependencies

<pre>
rust_prost_dependencies(<a href="#rust_prost_dependencies-bzlmod">bzlmod</a>)
</pre>

Declares repositories needed for prost.

**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="rust_prost_dependencies-bzlmod"></a>bzlmod |  Whether bzlmod is enabled.   |  `False` |

**RETURNS**

list[struct(repo=str, is_dev_dep=bool)]: A list of the repositories
  defined by this macro.


<a id="rust_prost_library"></a>

## rust_prost_library

<pre>
rust_prost_library(<a href="#rust_prost_library-name">name</a>, <a href="#rust_prost_library-kwargs">kwargs</a>)
</pre>

A rule for generating a Rust library using Prost.

**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="rust_prost_library-name"></a>name |  The name of the target.   |  none |
| <a id="rust_prost_library-kwargs"></a>kwargs |  Additional keyword arguments for the underlying `rust_prost_library` rule.   |  none |


<a id="rust_prost_transitive_repositories"></a>

## rust_prost_transitive_repositories

<pre>
rust_prost_transitive_repositories()
</pre>

Load transitive dependencies of the `@rules_rust_protobuf//` rules.

This macro should be called immediately after the `rust_protobuf_dependencies` macro.



<a id="rust_proto_protobuf_dependencies"></a>

## rust_proto_protobuf_dependencies

<pre>
rust_proto_protobuf_dependencies(<a href="#rust_proto_protobuf_dependencies-bzlmod">bzlmod</a>)
</pre>

Sets up dependencies for rules_rust's proto support.

**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="rust_proto_protobuf_dependencies-bzlmod"></a>bzlmod |  Whether this function is being called from a bzlmod context rather than a workspace context.   |  `False` |

**RETURNS**

A list of structs containing information about root module deps to report to bzlmod's extension_metadata.


<a id="rust_proto_protobuf_register_toolchains"></a>

## rust_proto_protobuf_register_toolchains

<pre>
rust_proto_protobuf_register_toolchains(<a href="#rust_proto_protobuf_register_toolchains-register_toolchains">register_toolchains</a>)
</pre>

Register toolchains for proto compilation.

**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="rust_proto_protobuf_register_toolchains-register_toolchains"></a>register_toolchains |  <p align="center"> - </p>   |  `True` |


<a id="rust_proto_protobuf_transitive_repositories"></a>

## rust_proto_protobuf_transitive_repositories

<pre>
rust_proto_protobuf_transitive_repositories()
</pre>

Load transitive dependencies of the `@rules_rust_protobuf//` rules.

This macro should be called immediately after the `rust_protobuf_dependencies` macro.
