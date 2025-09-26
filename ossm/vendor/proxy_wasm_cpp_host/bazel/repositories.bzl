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

load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository", "new_git_repository")
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")

def proxy_wasm_cpp_host_repositories():
    # Bazel extensions.

    # Update platforms for crate_universe. Can remove when we update Bazel version.
    maybe(
        http_archive,
        name = "platforms",
        urls = [
            "https://mirror.bazel.build/github.com/bazelbuild/platforms/releases/download/0.0.10/platforms-0.0.10.tar.gz",
            "https://github.com/bazelbuild/platforms/releases/download/0.0.10/platforms-0.0.10.tar.gz",
        ],
        sha256 = "218efe8ee736d26a3572663b374a253c012b716d8af0c07e842e82f238a0a7ee",
    )

    maybe(
        http_archive,
        name = "bazel_skylib",
        urls = [
            "https://mirror.bazel.build/github.com/bazelbuild/bazel-skylib/releases/download/1.1.1/bazel-skylib-1.1.1.tar.gz",
            "https://github.com/bazelbuild/bazel-skylib/releases/download/1.1.1/bazel-skylib-1.1.1.tar.gz",
        ],
        sha256 = "c6966ec828da198c5d9adbaa94c05e3a1c7f21bd012a0b29ba8ddbccb2c93b0d",
    )

    maybe(
        http_archive,
        name = "rules_cc",
        sha256 = "2037875b9a4456dce4a79d112a8ae885bbc4aad968e6587dca6e64f3a0900cdf",
        strip_prefix = "rules_cc-0.0.9",
        urls = ["https://github.com/bazelbuild/rules_cc/releases/download/0.0.9/rules_cc-0.0.9.tar.gz"],
    )

    maybe(
        http_archive,
        name = "bazel_clang_tidy",
        sha256 = "6ed23cbff9423a30ef10becf57210a26d54fe198a211f4037d931c06f843c023",
        strip_prefix = "bazel_clang_tidy-c2fe98cfec0430e78bff4169e9ca0a43123e4c99",
        url = "https://github.com/erenon/bazel_clang_tidy/archive/c2fe98cfec0430e78bff4169e9ca0a43123e4c99.tar.gz",
        patches = ["@proxy_wasm_cpp_host//bazel/external:bazel_clang_tidy.patch"],
        patch_args = ["-p1"],
    )

    maybe(
        http_archive,
        name = "bazel-zig-cc",
        sha256 = "ff89e0220c72cdc774e451a35e5c3b9f1593d0df71341844b2108c181ac0eef9",
        strip_prefix = "hermetic_cc_toolchain-0.4.4",
        url = "https://github.com/uber/hermetic_cc_toolchain/archive/refs/tags/v0.4.4.tar.gz",
    )

    maybe(
        http_archive,
        name = "rules_foreign_cc",
        sha256 = "bcd0c5f46a49b85b384906daae41d277b3dc0ff27c7c752cc51e43048a58ec83",
        strip_prefix = "rules_foreign_cc-0.7.1",
        url = "https://github.com/bazelbuild/rules_foreign_cc/archive/0.7.1.tar.gz",
    )

    maybe(
        http_archive,
        name = "rules_fuzzing",
        sha256 = "3ec0eee05b243552cc4a784b30323d088bf73cb2177ddda02c827e68981933f1",
        strip_prefix = "rules_fuzzing-0.5.2",
        urls = ["https://github.com/bazelbuild/rules_fuzzing/archive/v0.5.2.tar.gz"],
    )

    maybe(
        http_archive,
        name = "rules_python",
        sha256 = "778aaeab3e6cfd56d681c89f5c10d7ad6bf8d2f1a72de9de55b23081b2d31618",
        strip_prefix = "rules_python-0.34.0",
        url = "https://github.com/bazelbuild/rules_python/releases/download/0.34.0/rules_python-0.34.0.tar.gz",
    )

    # Keep at 0.42 one because https://github.com/bazelbuild/rules_rust/issues/2665
    # manifests at 0.43
    maybe(
        http_archive,
        name = "rules_rust",
        integrity = "sha256-JLN47ZcAbx9wEr5Jiib4HduZATGLiDgK7oUi/fvotzU=",
        # NOTE: Update Rust version in bazel/dependencies.bzl.
        url = "https://github.com/bazelbuild/rules_rust/releases/download/0.42.1/rules_rust-v0.42.1.tar.gz",
        patches = ["@proxy_wasm_cpp_host//bazel/external:rules_rust.patch"],
        patch_args = ["-p1"],
    )

    # Core deps. Keep them updated.

    # Note: we depend on Abseil via rules_fuzzing. Remove this pin when we update that.
    #
    # This is the latest LTS release, which picks up:
    # - Build fix: https://github.com/abseil/abseil-cpp/pull/1187
    # - A bugfix found in local fuzzing:
    #   https://github.com/abseil/abseil-cpp/commit/e7858c73279d81cbc005d9c76a385ab535520635
    maybe(
        http_archive,
        name = "com_google_absl",
        sha256 = "733726b8c3a6d39a4120d7e45ea8b41a434cdacde401cba500f14236c49b39dc",
        strip_prefix = "abseil-cpp-20240116.2",
        urls = ["https://github.com/abseil/abseil-cpp/archive/20240116.2.tar.gz"],
    )

    maybe(
        http_archive,
        name = "boringssl",
        # 2023-08-28 (master-with-bazel)
        sha256 = "f1f421738e9ba39dd88daf8cf3096ddba9c53e2b6b41b32fff5a3ff82f4cd162",
        strip_prefix = "boringssl-45cf810dbdbd767f09f8cb0b0fcccd342c39041f",
        urls = ["https://github.com/google/boringssl/archive/45cf810dbdbd767f09f8cb0b0fcccd342c39041f.tar.gz"],
    )

    maybe(
        http_archive,
        name = "proxy_wasm_cpp_sdk",
        sha256 = "89792fc1abca331f29f99870476a04146de5e82ff903bdffca90e6729c1f2470",
        strip_prefix = "proxy-wasm-cpp-sdk-95bb82ce45c41d9100fd1ec15d2ffc67f7f3ceee",
        urls = ["https://github.com/proxy-wasm/proxy-wasm-cpp-sdk/archive/95bb82ce45c41d9100fd1ec15d2ffc67f7f3ceee.tar.gz"],
    )

    # Compile DB dependencies.
    maybe(
        http_archive,
        name = "bazel_compdb",
        sha256 = "acd2a9eaf49272bb1480c67d99b82662f005b596a8c11739046a4220ec73c4da",
        strip_prefix = "bazel-compilation-database-40864791135333e1446a04553b63cbe744d358d0",
        url = "https://github.com/grailbio/bazel-compilation-database/archive/40864791135333e1446a04553b63cbe744d358d0.tar.gz",
    )

    # Test dependencies.

    maybe(
        http_archive,
        name = "com_google_googletest",
        sha256 = "9dc9157a9a1551ec7a7e43daea9a694a0bb5fb8bec81235d8a1e6ef64c716dcb",
        strip_prefix = "googletest-release-1.10.0",
        urls = ["https://github.com/google/googletest/archive/release-1.10.0.tar.gz"],
        patches = ["@proxy_wasm_cpp_host//bazel/external:googletest.patch"],
        patch_args = ["-p1"],
    )

    # NullVM dependencies.

    maybe(
        http_archive,
        name = "com_google_protobuf",
        sha256 = "77ad26d3f65222fd96ccc18b055632b0bfedf295cb748b712a98ba1ac0b704b2",
        strip_prefix = "protobuf-3.17.3",
        url = "https://github.com/protocolbuffers/protobuf/releases/download/v3.17.3/protobuf-all-3.17.3.tar.gz",
    )

    # V8 with dependencies.

    maybe(
        git_repository,
        name = "v8",
        # 10.7.193.13
        commit = "6c8b357a84847a479cd329478522feefc1c3195a",
        remote = "https://chromium.googlesource.com/v8/v8",
        shallow_since = "1664374400 +0000",
        patches = [
            "@proxy_wasm_cpp_host//bazel/external:v8.patch",
            "@proxy_wasm_cpp_host//bazel/external:v8_include.patch",
        ],
        patch_args = ["-p1"],
    )

    native.bind(
        name = "wee8",
        actual = "@v8//:wee8",
    )

    maybe(
        new_git_repository,
        name = "com_googlesource_chromium_base_trace_event_common",
        build_file = "@v8//:bazel/BUILD.trace_event_common",
        commit = "521ac34ebd795939c7e16b37d9d3ddb40e8ed556",
        remote = "https://chromium.googlesource.com/chromium/src/base/trace_event/common.git",
        shallow_since = "1662508800 +0000",
    )

    native.bind(
        name = "base_trace_event_common",
        actual = "@com_googlesource_chromium_base_trace_event_common//:trace_event_common",
    )

    # WAMR with dependencies.

    maybe(
        http_archive,
        name = "com_github_bytecodealliance_wasm_micro_runtime",
        build_file = "@proxy_wasm_cpp_host//bazel/external:wamr.BUILD",
        # WAMR-2.1.1
        sha256 = "a0824762abbcbb3dd6b7bb07530f198ece5d792a12a879bc2a99100590fdb151",
        strip_prefix = "wasm-micro-runtime-WAMR-2.1.1",
        url = "https://github.com/bytecodealliance/wasm-micro-runtime/archive/refs/tags/WAMR-2.1.1.zip",
    )

    native.bind(
        name = "wamr",
        actual = "@com_github_bytecodealliance_wasm_micro_runtime//:wamr_lib",
    )

    maybe(
        http_archive,
        name = "llvm-15_0_7",
        build_file = "@proxy_wasm_cpp_host//bazel/external:wamr_llvm.BUILD",
        sha256 = "8b5fcb24b4128cf04df1b0b9410ce8b1a729cb3c544e6da885d234280dedeac6",
        strip_prefix = "llvm-project-15.0.7.src",
        url = "https://github.com/llvm/llvm-project/releases/download/llvmorg-15.0.7/llvm-project-15.0.7.src.tar.xz",
    )

    # WasmEdge with dependencies.

    maybe(
        http_archive,
        name = "com_github_wasmedge_wasmedge",
        build_file = "@proxy_wasm_cpp_host//bazel/external:wasmedge.BUILD",
        sha256 = "7ab8a0df37c8d282ecff72d0f0bff8db63fd92df1645d5a014a9dbed4b7f9025",
        strip_prefix = "WasmEdge-proxy-wasm-0.13.1",
        url = "https://github.com/WasmEdge/WasmEdge/archive/refs/tags/proxy-wasm/0.13.1.tar.gz",
    )

    native.bind(
        name = "wasmedge",
        actual = "@com_github_wasmedge_wasmedge//:wasmedge_lib",
    )

    # Wasmtime with dependencies.

    maybe(
        http_archive,
        name = "com_github_bytecodealliance_wasmtime",
        build_file = "@proxy_wasm_cpp_host//bazel/external:wasmtime.BUILD",
        sha256 = "2ccb49bb3bfa4d86907ad4c80d1147aef6156c7b6e3f7f14ed02a39de9761155",
        strip_prefix = "wasmtime-24.0.0",
        url = "https://github.com/bytecodealliance/wasmtime/archive/v24.0.0.tar.gz",
    )

    native.bind(
        name = "wasmtime",
        actual = "@com_github_bytecodealliance_wasmtime//:wasmtime_lib",
    )

    native.bind(
        name = "prefixed_wasmtime",
        actual = "@com_github_bytecodealliance_wasmtime//:prefixed_wasmtime_lib",
    )

    # WAVM with dependencies.

    maybe(
        http_archive,
        name = "com_github_wavm_wavm",
        build_file = "@proxy_wasm_cpp_host//bazel/external:wavm.BUILD",
        sha256 = "7cfa3d7334c96f89553bb44eeee736a192826a78b4db114042d38d6882748f5b",
        strip_prefix = "WAVM-nightly-2022-05-14",
        url = "https://github.com/WAVM/WAVM/archive/refs/tags/nightly/2022-05-14.tar.gz",
    )

    native.bind(
        name = "wavm",
        actual = "@com_github_wavm_wavm//:wavm_lib",
    )

    maybe(
        http_archive,
        name = "llvm",
        build_file = "@proxy_wasm_cpp_host//bazel/external:llvm.BUILD",
        sha256 = "7d9a8405f557cefc5a21bf5672af73903b64749d9bc3a50322239f56f34ffddf",
        strip_prefix = "llvm-12.0.1.src",
        url = "https://github.com/llvm/llvm-project/releases/download/llvmorg-12.0.1/llvm-12.0.1.src.tar.xz",
        patches = ["@proxy_wasm_cpp_host//bazel/external:llvm.patch"],
        patch_args = ["-p1"],
    )
