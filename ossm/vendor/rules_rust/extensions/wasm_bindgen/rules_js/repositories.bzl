"""Rust wasm-bindgen rules_js dependencies."""

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")
load(
    "//:repositories.bzl",
    _rust_wasm_bindgen_dependencies = "rust_wasm_bindgen_dependencies",
    _rust_wasm_bindgen_register_toolchains = "rust_wasm_bindgen_register_toolchains",
)

def js_rust_wasm_bindgen_dependencies():
    _rust_wasm_bindgen_dependencies()

    maybe(
        http_archive,
        name = "aspect_rules_js",
        sha256 = "75c25a0f15a9e4592bbda45b57aa089e4bf17f9176fd735351e8c6444df87b52",
        strip_prefix = "rules_js-2.1.0",
        url = "https://github.com/aspect-build/rules_js/releases/download/v2.1.0/rules_js-v2.1.0.tar.gz",
    )

def js_rust_wasm_bindgen_register_toolchains(**kwargs):
    _rust_wasm_bindgen_register_toolchains(**kwargs)
