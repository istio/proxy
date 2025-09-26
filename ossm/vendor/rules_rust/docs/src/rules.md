# Rules

- [defs](defs.md): standard rust rules for building and testing libraries and binaries.
- [rustdoc](rust_doc.md): rules for generating and testing rust documentation.
- [clippy](rust_clippy.md): rules for running [clippy](https://github.com/rust-lang/rust-clippy#readme).
- [rustfmt](rust_fmt.md): rules for running [rustfmt](https://github.com/rust-lang/rustfmt#readme).
- [cargo](cargo.md): Rules dedicated to Cargo compatibility. ie: [`build.rs` scripts](https://doc.rust-lang.org/cargo/reference/build-scripts.html).
- [crate_universe](external_crates.md): Rules for generating Bazel targets for external crate dependencies.

## Experimental rules

- [rust_analyzer](rust_analyzer.md): rules for generating `rust-project.json` files for [rust-analyzer](https://rust-analyzer.github.io/)

## 3rd party rules

- [rust_bindgen](rust_bindgen.md): rules for generating C++ bindings.
- [rust_prost](rust_prost.md): rules for generating [protobuf](https://developers.google.com/protocol-buffers) and [gRPC](https://grpc.io) stubs using [prost](https://github.com/tokio-rs/prost).
- [rust_protobuf](rust_protobuf.md): rules for generating [protobuf](https://developers.google.com/protocol-buffers) and [gRPC](https://grpc.io) stubs with [rust-protobuf](https://github.com/stepancheg/rust-protobuf/)
- [rust_wasm_bindgen](rust_wasm_bindgen.md): rules for generating [WebAssembly](https://www.rust-lang.org/what/wasm) bindings.

## Full API

You can also browse the [full API in one page](flatten.md).
