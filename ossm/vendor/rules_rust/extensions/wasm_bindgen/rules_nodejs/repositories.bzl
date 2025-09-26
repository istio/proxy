"""Rust wasm-bindgen rules_nodejs dependencies."""

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")
load(
    "//:repositories.bzl",
    _rust_wasm_bindgen_dependencies = "rust_wasm_bindgen_dependencies",
    _rust_wasm_bindgen_register_toolchains = "rust_wasm_bindgen_register_toolchains",
)

def nodejs_rust_wasm_bindgen_dependencies():
    _rust_wasm_bindgen_dependencies()

    maybe(
        http_archive,
        name = "rules_nodejs",
        sha256 = "8fc8e300cb67b89ceebd5b8ba6896ff273c84f6099fc88d23f24e7102319d8fd",
        urls = ["https://github.com/bazelbuild/rules_nodejs/releases/download/5.8.4/rules_nodejs-core-5.8.4.tar.gz"],
    )

def nodejs_rust_wasm_bindgen_register_toolchains(**kwargs):
    _rust_wasm_bindgen_register_toolchains(**kwargs)
