# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

## [0.2.2] - 2024-07-21

### Fixed

- Fixed support for nested gRPC callouts.
  Thanks [@andytesti](https://github.com/andytesti)!

- Fixed panic on unknown `token_id` in `on_grpc_receive_initial_metadata`
  and `on_grpc_receive_trailing_metadata`.
  Thanks [@erikness-doordash](https://github.com/erikness-doordash)!

- Fixed panic on unexpected failures in `get_property`.
  Thanks [@alexsnaps](https://github.com/alexsnaps)!

- Fixed panic on unexpected failures in `call_foreign_function`.
  Reported by [@geNAZt](https://github.com/geNAZt).

### Added

- Added support for sending error responses with gRPC status codes.
  Thanks [@juanmolle](https://github.com/juanmolle)!

## [0.2.1] - 2022-11-22

### Fixed

- Fixed panic on unknown `token_id` in `on_grpc_close`.
  Thanks [@Protryon](https://github.com/Protryon)!

### Changed

- Changed MSRV to v1.61.0.

### Removed

- Removed `wee-alloc` feature, because that crate is no longer maintained
  and it leaks memory.

## [0.2.0] - 2022-04-08

### Fixed

- Fixed performance degradation with `wasm32-wasi` target in Rust v1.56.0
  or newer by adding `proxy_wasm::main` macro that should be used instead
  of custom `_start`, `_initialize` and/or `main` exports.

### Changed

- Updated ABI to Proxy-Wasm ABI v0.2.1.

### Added

- Added support for calling foreign functions.
  Thanks [@Gsantomaggio](https://github.com/Gsantomaggio)!

## [0.1.4] - 2021-07-01

### Added

- Added support for gRPC callouts.
  Thanks [@Shikugawa](https://github.com/Shikugawa)!

## [0.1.3] - 2020-12-04

### Fixed

- Fixed support for nested HTTP callouts.
  Thanks [@SvetlinZarev](https://github.com/SvetlinZarev)!

### Changed

- Changed `wee-alloc` to an optional feature.
  Thanks [@yuval-k](https://github.com/yuval-k)!

### Added

- Added support for building for `wasm32-wasi` target.
- Added support for metrics.
- Added support for `RootContext` to create child contexts for streams.
  Thanks [@dgn](https://github.com/dgn)!
- Added support for setting network buffers.

## [0.1.2] - 2020-08-05

### Changed

- Updated `MapType` values to match updated Proxy-Wasm ABI v0.1.0.
  Thanks [@yskopets](https://github.com/yskopets)!

## [0.1.1] - 2020-08-05

### Added

- Added support for building with Bazel.
- Added support for setting HTTP bodies.
  Thanks [@gbrail](https://github.com/gbrail)!

## [0.1.0] - 2020-02-29

### Added

- Initial release.


[0.2.2]: https://github.com/proxy-wasm/proxy-wasm-rust-sdk/compare/v0.2.1...v0.2.2
[0.2.1]: https://github.com/proxy-wasm/proxy-wasm-rust-sdk/compare/v0.2.0...v0.2.1
[0.2.0]: https://github.com/proxy-wasm/proxy-wasm-rust-sdk/compare/v0.1.4...v0.2.0
[0.1.4]: https://github.com/proxy-wasm/proxy-wasm-rust-sdk/compare/v0.1.3...v0.1.4
[0.1.3]: https://github.com/proxy-wasm/proxy-wasm-rust-sdk/compare/v0.1.2...v0.1.3
[0.1.2]: https://github.com/proxy-wasm/proxy-wasm-rust-sdk/compare/v0.1.1...v0.1.2
[0.1.1]: https://github.com/proxy-wasm/proxy-wasm-rust-sdk/compare/v0.1.0...v0.1.1
[0.1.0]: https://github.com/proxy-wasm/proxy-wasm-rust-sdk/releases/tag/v0.1.0
