load("@rules_cc//cc:defs.bzl", "cc_library")
load("@rules_rust//rust:defs.bzl", "rust_static_library")

licenses(["notice"])  # Apache 2

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "wasmtime_lib",
    hdrs = [
        "crates/c-api/include/wasm.h",
    ],
    deps = [
        ":rust_c_api",
    ],
)

genrule(
    name = "prefixed_wasmtime_c_api_headers",
    srcs = [
        "crates/c-api/include/wasm.h",
    ],
    outs = [
        "crates/c-api/include/prefixed_wasm.h",
    ],
    cmd = """
        sed -e 's/\\ wasm_/\\ wasmtime_wasm_/g' \
            -e 's/\\*wasm_/\\*wasmtime_wasm_/g' \
            -e 's/(wasm_/(wasmtime_wasm_/g'     \
        $(<) >$@
        """,
)

genrule(
    name = "prefixed_wasmtime_c_api_lib",
    srcs = [
        ":rust_c_api",
    ],
    outs = [
        "prefixed_wasmtime_c_api.a",
    ],
    cmd = """
        for symbol in $$(nm -P $(<) 2>/dev/null | grep -E ^_?wasm_ | cut -d" " -f1); do
            echo $$symbol | sed -r 's/^(_?)(wasm_[a-z_]+)$$/\\1\\2 \\1wasmtime_\\2/' >>prefixed
        done
        # This should be OBJCOPY, but bazel-zig-cc doesn't define it.
        objcopy --redefine-syms=prefixed $(<) $@
        """,
    toolchains = ["@bazel_tools//tools/cpp:current_cc_toolchain"],
)

cc_library(
    name = "prefixed_wasmtime_lib",
    srcs = [
        ":prefixed_wasmtime_c_api_lib",
    ],
    hdrs = [
        ":prefixed_wasmtime_c_api_headers",
    ],
    linkstatic = 1,
)

rust_static_library(
    name = "rust_c_api",
    srcs = glob(["crates/c-api/src/**/*.rs"]),
    crate_features = ["cranelift"],
    crate_root = "crates/c-api/src/lib.rs",
    edition = "2021",
    proc_macro_deps = [
        "@proxy_wasm_cpp_host//bazel/cargo/wasmtime/remote:wasmtime-c-api-macros",
    ],
    deps = [
        "@proxy_wasm_cpp_host//bazel/cargo/wasmtime/remote:anyhow",
        "@proxy_wasm_cpp_host//bazel/cargo/wasmtime/remote:env_logger",
        "@proxy_wasm_cpp_host//bazel/cargo/wasmtime/remote:once_cell",
        # buildifier: leave-alone
        "@proxy_wasm_cpp_host//bazel/cargo/wasmtime/remote:wasmtime",
    ],
)
