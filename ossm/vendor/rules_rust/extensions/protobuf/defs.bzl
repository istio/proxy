"""# rules_rust_protobuf

These build rules are used for building [protobufs][protobuf]/[gRPC][grpc] in [Rust][rust] with Bazel
using [`rust-protobuf`].

[rust]: http://www.rust-lang.org/
[protobuf]: https://developers.google.com/protocol-buffers/
[grpc]: https://grpc.io
[`rust-protobuf`]: https://github.com/stepancheg/rust-protobuf/

## Setup

To use the Rust proto rules, add the following to your `WORKSPACE` file to add the
external repositories for the Rust proto toolchain (in addition to the [rust rules setup](..)):

```python
load("@rules_rust//proto/protobuf:repositories.bzl", "rust_proto_protobuf_dependencies", "rust_proto_protobuf_register_toolchains")

rust_proto_protobuf_dependencies()

rust_proto_protobuf_register_toolchains()

load("@rules_rust//proto/protobuf:transitive_repositories.bzl", "rust_proto_protobuf_transitive_repositories")

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
[@rules_rust//proto/protobuf/3rdparty/BUILD.bazel](https://github.com/bazelbuild/rules_rust/blob/main/proto/protobuf/3rdparty/BUILD.bazel)
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
    toolchain_type = "@rules_rust//proto/protobuf:toolchain_type",
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
"""

load(
    ":proto.bzl",
    _rust_grpc_library = "rust_grpc_library",
    _rust_proto_library = "rust_proto_library",
)

rust_grpc_library = _rust_grpc_library
rust_proto_library = _rust_proto_library
