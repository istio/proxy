# Copyright 2020 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

load("@aspect_rules_lint//format:repositories.bzl", "rules_lint_dependencies")
load("@bazel_lib//lib:repositories.bzl", "bazel_lib_dependencies", "bazel_lib_register_toolchains")
load("@com_google_googletest//:googletest_deps.bzl", "googletest_deps")
load("@com_google_protobuf//:protobuf_deps.bzl", "protobuf_deps")
load("@envoy_toolshed//sysroot:sysroot.bzl", "setup_sysroots")
load("@llvm-raw//utils/bazel:configure.bzl", "llvm_configure")
load("@proxy_wasm_cpp_host//bazel/cargo/wasmsign/remote:crates.bzl", wasmsign_crate_repositories = "crate_repositories")
load("@proxy_wasm_cpp_host//bazel/cargo/wasmtime/remote:crates.bzl", wasmtime_crate_repositories = "crate_repositories")
load("@rules_foreign_cc//foreign_cc:repositories.bzl", "rules_foreign_cc_dependencies")
load("@rules_python//python:repositories.bzl", "py_repositories")
load("@rules_rust//crate_universe:repositories.bzl", "crate_universe_dependencies")
load("@rules_rust//rust:repositories.bzl", "rust_repositories", "rust_repository_set")
load("@toolchains_llvm//toolchain:deps.bzl", "bazel_toolchain_dependencies")
load("@toolchains_llvm//toolchain:rules.bzl", "llvm_toolchain")

def proxy_wasm_cpp_host_dependencies():
    # Bazel extensions.
    googletest_deps()
    rules_foreign_cc_dependencies()

    rules_lint_dependencies()

    bazel_lib_dependencies()
    bazel_lib_register_toolchains()

    py_repositories()

    rust_repositories()
    rust_repository_set(
        name = "rust_linux_x86_64",
        exec_triple = "x86_64-unknown-linux-gnu",
        extra_target_triples = [
            "aarch64-unknown-linux-gnu",
            "wasm32-unknown-unknown",
            "wasm32-wasip1",
        ],
        versions = ["1.91.0"],
        sha256s = {
            "rustc-1.91.0-x86_64-unknown-linux-gnu.tar.xz": "a7169e8cb6174af2f45717703370363d8de82ce55f6ccba185893045b9370874",
            "clippy-1.91.0-x86_64-unknown-linux-gnu.tar.xz": "0087c3d58d2fdeafa89830c299b1026c9f981b49835db89c922b3c6a299b3225",
            "cargo-1.91.0-x86_64-unknown-linux-gnu.tar.xz": "7103c03fb8abe85b23307005a9dfe4f01c826a89945d84b96fa2d03fd4d2d138",
            "llvm-tools-1.91.0-x86_64-unknown-linux-gnu.tar.xz": "3d7756e0ce1ee18d20399aef770f09c55a4134f5a0f18e4e3c018aa1b3919a70",
            "rust-std-1.91.0-x86_64-unknown-linux-gnu.tar.xz": "89e6520b16c12b43526440298d2da0dcb70747c5cc2d0b8e47d39b5da9aeef49",
        },
    )
    rust_repository_set(
        name = "rust_linux_s390x",
        exec_triple = "s390x-unknown-linux-gnu",
        extra_target_triples = [
            "wasm32-unknown-unknown",
            "wasm32-wasip1",
        ],
        versions = ["1.91.0"],
    )
    crate_universe_dependencies(bootstrap = True)

    setup_sysroots()
    bazel_toolchain_dependencies()
    llvm_toolchain(
        name = "llvm_toolchain",
        llvm_version = "19.1.0",
        sha256 = {
            "linux-x86_64": "cee77d641690466a193d9b88c89705de1c02bbad46bde6a3b126793c0a0f2923",
            "linux-aarch64": "7bb54afd330fe1a1c2d4c593fa1e2dbe2abd9bf34fb3597994ff41e443cf144b",
            "darwin-aarch64": "9da86f64a99f5ce9b679caf54e938736ca269c5e069d0c94ad08b995c5f25c16",
            "darwin-x86_64": "264f2f1e8b67f066749349ae8b4943d346cd44e099464164ef21b42a57663540",
        },
        strip_prefix = {
            "linux-x86_64": "LLVM-19.1.0-Linux-X64",
            "linux-aarch64": "clang+llvm-19.1.0-aarch64-linux-gnu",
            "darwin-aarch64": "LLVM-19.1.0-macOS-ARM64",
            "darwin-x86_64": "LLVM-19.1.0-macOS-X64",
        },
        urls = {
            "linux-x86_64": ["https://github.com/llvm/llvm-project/releases/download/llvmorg-19.1.0/LLVM-19.1.0-Linux-X64.tar.xz"],
            "linux-aarch64": ["https://github.com/llvm/llvm-project/releases/download/llvmorg-19.1.0/clang+llvm-19.1.0-aarch64-linux-gnu.tar.xz"],
            "darwin-aarch64": ["https://github.com/llvm/llvm-project/releases/download/llvmorg-19.1.0/LLVM-19.1.0-macOS-ARM64.tar.xz"],
            "darwin-x86_64": ["https://github.com/llvm/llvm-project/releases/download/llvmorg-19.1.0/LLVM-19.1.0-macOS-X64.tar.xz"],
        },
    )

    llvm_toolchain(
        name = "llvm_aarch64",
        llvm_version = "19.1.0",
        toolchain_roots = {
            "": "@llvm_toolchain_llvm//",
        },
        sysroot = {
            "linux-aarch64": "@sysroot_linux_arm64//:sysroot",
        },
    )

    # NullVM dependencies.

    protobuf_deps()

    # LLVM with native Bazel build (for WAMR JIT).
    # Configure LLVM using its native Bazel overlay from utils/bazel.
    # Only build X86 and AArch64 targets to minimize build size.
    # Note: LLVM external dependencies (llvm_zlib, llvm_zstd) are defined in repositories.bzl
    llvm_configure(name = "llvm-project", targets = ["X86", "AArch64"])

    # Wasmtime dependencies.

    wasmtime_crate_repositories()

    # Test dependencies.

    wasmsign_crate_repositories()
