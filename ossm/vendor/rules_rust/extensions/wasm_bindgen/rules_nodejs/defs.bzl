"""Rust WASM-bindgen rules for interfacing with bazelbuild/rules_nodejs"""

load("@rules_nodejs//nodejs:providers.bzl", "DeclarationInfo", "JSModuleInfo")

# buildifier: disable=bzl-visibility
load("@rules_rust//rust/private:providers.bzl", "RustAnalyzerGroupInfo", "RustAnalyzerInfo")
load("//private:wasm_bindgen.bzl", "WASM_BINDGEN_ATTR", "rust_wasm_bindgen_action")

def _nodejs_rust_wasm_bindgen_impl(ctx):
    toolchain = ctx.toolchains[Label("//:toolchain_type")]

    info = rust_wasm_bindgen_action(
        ctx = ctx,
        toolchain = toolchain,
        wasm_file = ctx.attr.wasm_file,
        target_output = ctx.attr.target,
        flags = ctx.attr.bindgen_flags,
    )

    # Return a structure that is compatible with the deps[] of a ts_library.
    declarations = info.ts
    es5_sources = info.js

    providers = [
        DefaultInfo(
            files = depset([info.wasm], transitive = [info.js, info.ts]),
        ),
        DeclarationInfo(
            declarations = declarations,
            transitive_declarations = declarations,
            type_blocklisted_declarations = depset([]),
        ),
        JSModuleInfo(
            direct_sources = es5_sources,
            sources = es5_sources,
        ),
        info,
    ]

    if RustAnalyzerGroupInfo in ctx.attr.wasm_file:
        providers.append(ctx.attr.wasm_file[RustAnalyzerGroupInfo])

    if RustAnalyzerInfo in ctx.attr.wasm_file:
        providers.append(ctx.attr.wasm_file[RustAnalyzerInfo])

    return providers

nodejs_rust_wasm_bindgen = rule(
    doc = """\
Generates javascript and typescript bindings for a webassembly module using [wasm-bindgen][ws] that interface with [bazelbuild/rules_nodejs][bbnjs].

[ws]: https://rustwasm.github.io/docs/wasm-bindgen/
[bbnjs]: https://github.com/bazelbuild/rules_nodejs
""",
    implementation = _nodejs_rust_wasm_bindgen_impl,
    attrs = WASM_BINDGEN_ATTR,
    toolchains = [
        str(Label("//:toolchain_type")),
    ],
)
