# WebAssembly for Proxies (C++ SDK)

[![Build Status][build-badge]][build-link]
[![Apache 2.0 License][license-badge]][license-link]

Proxy-Wasm is a specification and supporting framework for using
[WebAssembly](https://webassembly.org) (Wasm) to extend the functionality of
network proxies. It enables developers to write custom logic (plugins) that are
compiled to Wasm modules and then loaded and executed by the proxy.

Proxy-Wasm consists of multiple parts:

* An [ABI](https://github.com/proxy-wasm/spec) that specifies the low-level
  interface between network proxies and Wasm virtual machines that run the
  plugins.
* [Host implementations](https://github.com/proxy-wasm/spec#host-environments)
  of the ABI, provided by network proxies.
* [Language-specific SDKs](https://github.com/proxy-wasm/spec#sdks) that layer
  on top of the ABI, providing a more natural and programmer-friendly API for
  invoking and implementing Proxy-Wasm functions and callbacks.
  
This repository provides the C++ SDK.

## Getting started

* Read the [API overview](docs/api_overview.md) to learn about [Proxy-Wasm
  concepts](docs/api_overview.md#concepts-and-terminology) and how they are
  represented in the C++ SDK.
* View an [example plugin](example/http_wasm_example.cc).
* Refer to [API documentation](docs/api_overview.md#codemap).
* [Build](docs/building.md) plugin code.

[build-badge]: https://github.com/proxy-wasm/proxy-wasm-cpp-sdk/workflows/C++/badge.svg?branch=master
[build-link]: https://github.com/proxy-wasm/proxy-wasm-cpp-sdk/actions?query=workflow%3AC%2B%2B+branch%3Amaster
[license-badge]: https://img.shields.io/github/license/proxy-wasm/proxy-wasm-cpp-sdk
[license-link]: https://github.com/proxy-wasm/proxy-wasm-cpp-sdk/blob/master/LICENSE
