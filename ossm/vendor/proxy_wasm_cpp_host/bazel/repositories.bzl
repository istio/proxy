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

    maybe(
        http_archive,
        name = "bazel_features",
        sha256 = "c41853e3b636c533b86bf5ab4658064e6cc9db0a3bce52cbff0629e094344ca9",
        strip_prefix = "bazel_features-1.33.0",
        urls = ["https://github.com/bazel-contrib/bazel_features/releases/download/v1.33.0/bazel_features-v1.33.0.tar.gz"],
    )

    # Update platforms for crate_universe. Can remove when we update Bazel version.
    maybe(
        http_archive,
        name = "platforms",
        urls = [
            "https://mirror.bazel.build/github.com/bazelbuild/platforms/releases/download/1.0.0/platforms-1.0.0.tar.gz",
            "https://github.com/bazelbuild/platforms/releases/download/1.0.0/platforms-1.0.0.tar.gz",
        ],
        sha256 = "3384eb1c30762704fbe38e440204e114154086c8fc8a8c2e3e28441028c019a8",
    )

    maybe(
        http_archive,
        name = "bazel_skylib",
        urls = [
            "https://mirror.bazel.build/github.com/bazelbuild/bazel-skylib/releases/download/1.7.0/bazel-skylib-1.7.0.tar.gz",
            "https://github.com/bazelbuild/bazel-skylib/releases/download/1.7.0/bazel-skylib-1.7.0.tar.gz",
        ],
        sha256 = "d00f1389ee20b60018e92644e0948e16e350a7707219e7a390fb0a99b6ec9262",
    )

    maybe(
        http_archive,
        name = "rules_cc",
        sha256 = "b8b918a85f9144c01f6cfe0f45e4f2838c7413961a8ff23bc0c6cdf8bb07a3b6",
        strip_prefix = "rules_cc-0.1.5",
        url = "https://github.com/bazelbuild/rules_cc/releases/download/0.1.5/rules_cc-0.1.5.tar.gz",
    )

    # aspect_rules_lint v1.12.0 for modern clang-tidy integration
    maybe(
        http_archive,
        name = "aspect_rules_lint",
        sha256 = "a8a63bd071a39bd5be1f99d9f258eac674673c98505f9fc5b4c76587f67278cd",
        strip_prefix = "rules_lint-1.12.0",
        url = "https://github.com/aspect-build/rules_lint/releases/download/v1.12.0/rules_lint-v1.12.0.tar.gz",
    )

    # bazel_lib v3.0.1 required by aspect_rules_lint v1.12.0
    maybe(
        http_archive,
        name = "bazel_lib",
        sha256 = "8b074b1a2731d29f6b95defdca95297354dc424492caf7019cf6b9f36afba54f",
        strip_prefix = "bazel-lib-3.0.1",
        url = "https://github.com/bazel-contrib/bazel-lib/releases/download/v3.0.1/bazel-lib-v3.0.1.tar.gz",
    )

    # aspect_bazel_lib alias for aspect_rules_js v2.1.2 compatibility
    # aspect_rules_js expects @aspect_bazel_lib while aspect_rules_lint expects @bazel_lib
    # Both repos reference the same bazel-lib v3.0.1 release to maintain consistency
    maybe(
        http_archive,
        name = "aspect_bazel_lib",
        sha256 = "8b074b1a2731d29f6b95defdca95297354dc424492caf7019cf6b9f36afba54f",
        strip_prefix = "bazel-lib-3.0.1",
        url = "https://github.com/bazel-contrib/bazel-lib/releases/download/v3.0.1/bazel-lib-v3.0.1.tar.gz",
    )

    # aspect_rules_js is required by aspect_rules_lint
    maybe(
        http_archive,
        name = "aspect_rules_js",
        sha256 = "fbc34d815a0cc52183a1a26732fc0329e26774a51abbe0f26fc9fd2dab6133b4",
        strip_prefix = "rules_js-2.1.2",
        url = "https://github.com/aspect-build/rules_js/releases/download/v2.1.2/rules_js-v2.1.2.tar.gz",
    )

    maybe(
        http_archive,
        name = "envoy_toolshed",
        sha256 = "e2252e46e64417d5cedd9f1eb34a622bce5e13b43837e5fe051c83066b0a400b",
        strip_prefix = "toolshed-bazel-bins-v0.1.13/bazel",
        url = "https://github.com/envoyproxy/toolshed/archive/refs/tags/bazel-bins-v0.1.13.tar.gz",
    )
    maybe(
        http_archive,
        name = "toolchains_llvm",
        sha256 = "fded02569617d24551a0ad09c0750dc53a3097237157b828a245681f0ae739f8",
        strip_prefix = "toolchains_llvm-v1.4.0",
        canonical_id = "v1.4.0",
        url = "https://github.com/bazel-contrib/toolchains_llvm/releases/download/v1.4.0/toolchains_llvm-v1.4.0.tar.gz",
    )

    maybe(
        http_archive,
        name = "rules_foreign_cc",
        sha256 = "32759728913c376ba45b0116869b71b68b1c2ebf8f2bcf7b41222bc07b773d73",
        strip_prefix = "rules_foreign_cc-0.15.1",
        url = "https://github.com/bazel-contrib/rules_foreign_cc/releases/download/0.15.1/rules_foreign_cc-0.15.1.tar.gz",
    )

    maybe(
        http_archive,
        name = "rules_fuzzing",
        sha256 = "850897989ebc06567ea06c959eb4a6129fa509ed2dbbd0d147d62d2b986714a9",
        strip_prefix = "rules_fuzzing-0.6.0",
        urls = ["https://github.com/bazelbuild/rules_fuzzing/archive/v0.6.0.tar.gz"],
    )

    maybe(
        http_archive,
        name = "rules_python",
        sha256 = "f2e80f97f9c0b82e2489e61e725df1e6bdaf16c4dacf5e26b95668787164baff",
        strip_prefix = "rules_python-1.6.1",
        url = "https://github.com/bazel-contrib/rules_python/releases/download/1.6.1/rules_python-1.6.1.tar.gz",
    )

    maybe(
        http_archive,
        name = "rules_rust",
        integrity = "sha256-yKqAbPYGZnmsI0YyQe6ArWkiZdrQRl9RERy74wuJA1I=",
        urls = ["https://github.com/bazelbuild/rules_rust/releases/download/0.68.1/rules_rust-0.68.1.tar.gz"],
        patches = ["@proxy_wasm_cpp_host//bazel/external:rules_rust.patch"],
        patch_args = ["-p1"],
    )

    # Core deps. Keep them updated.

    # Note: we depend on Abseil via rules_fuzzing. Remove this pin when we update that.
    #
    # This is the latest LTS release (20250512.1), required by protobuf 33.2
    maybe(
        http_archive,
        name = "com_google_absl",
        sha256 = "9b7a064305e9fd94d124ffa6cc358592eb42b5da588fb4e07d09254aa40086db",
        strip_prefix = "abseil-cpp-20250512.1",
        urls = ["https://github.com/abseil/abseil-cpp/archive/refs/tags/20250512.1.tar.gz"],
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
        sha256 = "26c4c0f9f645de7e789dc92f113d7352ee54ac43bb93ae3a8a22945f1ce71590",
        strip_prefix = "proxy-wasm-cpp-sdk-7465dee8b2953beebff99f6dc3720ad0c79bab99",
        urls = ["https://github.com/proxy-wasm/proxy-wasm-cpp-sdk/archive/7465dee8b2953beebff99f6dc3720ad0c79bab99.tar.gz"],
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
        sha256 = "65fab701d9829d38cb77c14acdc431d2108bfdbf8979e40eb8ae567edf10b27c",
        strip_prefix = "googletest-1.17.0",
        urls = ["https://github.com/google/googletest/releases/download/v1.17.0/googletest-1.17.0.tar.gz"],
        repo_mapping = {
            "@abseil-cpp": "@com_google_absl",
        },
    )

    # NullVM dependencies.

    maybe(
        http_archive,
        name = "com_google_protobuf",
        sha256 = "6b6599b54c88d75904b7471f5ca34a725fa0af92e134dd1a32d5b395aa4b4ca8",
        strip_prefix = "protobuf-33.2",
        url = "https://github.com/protocolbuffers/protobuf/releases/download/v33.2/protobuf-33.2.tar.gz",
        repo_mapping = {
            "@abseil-cpp": "@com_google_absl",
        },
    )

    # V8 with dependencies.

    maybe(
        git_repository,
        name = "v8",
        # 13.8.258.26
        commit = "de9d0f8b56ae61896e4d2ac577fc589efb14f87d",
        remote = "https://chromium.googlesource.com/v8/v8",
        shallow_since = "1752074621 -0700",
        patches = [
            "@proxy_wasm_cpp_host//bazel/external:v8.patch",
        ],
        patch_args = ["-p1"],
        patch_cmds = [
            "find ./src ./include -type f -exec sed -i.bak -e 's!#include \"third_party/simdutf/simdutf.h\"!#include \"simdutf.h\"!' {} \\;",
            "find ./src ./include -type f -exec sed -i.bak -e 's!#include \"third_party/fp16/src/include/fp16.h\"!#include \"fp16.h\"!' {} \\;",
            "find ./src ./include -type f -exec sed -i.bak -e 's!#include \"third_party/dragonbox/src/include/dragonbox/dragonbox.h\"!#include \"dragonbox/dragonbox.h\"!' {} \\;",
            "find ./src ./include -type f -exec sed -i.bak -e 's!#include \"third_party/fast_float/src/include/fast_float/!#include \"fast_float/!' {} \\;",
        ],
        repo_mapping = {
            "@abseil-cpp": "@com_google_absl",
        },
    )

    maybe(
        http_archive,
        name = "highway",
        sha256 = "7e0be78b8318e8bdbf6fa545d2ecb4c90f947df03f7aadc42c1967f019e63343",
        urls = [
            "https://github.com/google/highway/archive/refs/tags/1.2.0.tar.gz",
        ],
        strip_prefix = "highway-1.2.0",
    )

    maybe(
        http_archive,
        name = "fast_float",
        sha256 = "d2a08e722f461fe699ba61392cd29e6b23be013d0f56e50c7786d0954bffcb17",
        urls = [
            "https://github.com/fastfloat/fast_float/archive/refs/tags/v7.0.0.tar.gz",
        ],
        strip_prefix = "fast_float-7.0.0",
    )

    maybe(
        http_archive,
        name = "dragonbox",
        urls = [
            "https://github.com/jk-jeon/dragonbox/archive/6c7c925b571d54486b9ffae8d9d18a822801cbda.zip",
        ],
        strip_prefix = "dragonbox-6c7c925b571d54486b9ffae8d9d18a822801cbda",
        sha256 = "2f10448d665355b41f599e869ac78803f82f13b070ce7ef5ae7b5cceb8a178f3",
        build_file = "@proxy_wasm_cpp_host//bazel/external:dragonbox.BUILD",
    )

    maybe(
        http_archive,
        name = "fp16",
        urls = [
            "https://github.com/Maratyszcza/FP16/archive/0a92994d729ff76a58f692d3028ca1b64b145d91.zip",
        ],
        strip_prefix = "FP16-0a92994d729ff76a58f692d3028ca1b64b145d91",
        sha256 = "e66e65515fa09927b348d3d584c68be4215cfe664100d01c9dbc7655a5716d70",
        build_file = "@proxy_wasm_cpp_host//bazel/external:fp16.BUILD",
    )

    maybe(
        http_archive,
        name = "simdutf",
        sha256 = "512374f8291d3daf102ccd0ad223b1a8318358f7c1295efd4d9a3abbb8e4b6ff",
        urls = [
            "https://github.com/simdutf/simdutf/releases/download/v7.3.0/singleheader.zip",
        ],
        build_file = "@proxy_wasm_cpp_host//bazel/external:simdutf.BUILD",
    )

    maybe(
        http_archive,
        name = "intel_ittapi",
        strip_prefix = "ittapi-a3911fff01a775023a06af8754f9ec1e5977dd97",
        sha256 = "1d0dddfc5abb786f2340565c82c6edd1cff10c917616a18ce62ee0b94dbc2ed4",
        urls = ["https://github.com/intel/ittapi/archive/a3911fff01a775023a06af8754f9ec1e5977dd97.tar.gz"],
        build_file = "@proxy_wasm_cpp_host//bazel/external:intel_ittapi.BUILD",
    )

    # WAMR with dependencies.

    maybe(
        http_archive,
        name = "com_github_bytecodealliance_wasm_micro_runtime",
        build_file = "@proxy_wasm_cpp_host//bazel/external:wamr.BUILD",
        # WAMR-2.4.1
        sha256 = "ca18bbf304f47287bf43707564db63b8908dd6d0d6ac40bb39271a7144def4cc",
        strip_prefix = "wasm-micro-runtime-WAMR-2.4.1",
        url = "https://github.com/bytecodealliance/wasm-micro-runtime/archive/refs/tags/WAMR-2.4.1.zip",
        patches = ["@proxy_wasm_cpp_host//bazel/external:wamr.patch"],
        patch_args = ["-p1"],
    )

    maybe(
        http_archive,
        name = "llvm-raw",
        build_file = "@proxy_wasm_cpp_host//bazel/external:wamr_llvm.BUILD",
        sha256 = "5042522b49945bc560ff9206f25fb87980a9b89b914193ca00d961511ff0673c",
        strip_prefix = "llvm-project-19.1.0.src",
        url = "https://github.com/llvm/llvm-project/releases/download/llvmorg-19.1.0/llvm-project-19.1.0.src.tar.xz",
    )

    # LLVM external dependencies for native Bazel build.
    maybe(
        http_archive,
        name = "llvm_zlib",
        build_file = "@llvm-raw//utils/bazel/third_party_build:zlib-ng.BUILD",
        sha256 = "e36bb346c00472a1f9ff2a0a4643e590a254be6379da7cddd9daeb9a7f296731",
        strip_prefix = "zlib-ng-2.0.7",
        urls = [
            "https://github.com/zlib-ng/zlib-ng/archive/refs/tags/2.0.7.zip",
        ],
    )

    maybe(
        http_archive,
        name = "llvm_zstd",
        build_file = "@llvm-raw//utils/bazel/third_party_build:zstd.BUILD",
        sha256 = "7c42d56fac126929a6a85dbc73ff1db2411d04f104fae9bdea51305663a83fd0",
        strip_prefix = "zstd-1.5.2",
        urls = [
            "https://github.com/facebook/zstd/releases/download/v1.5.2/zstd-1.5.2.tar.gz",
        ],
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

    # Wasmtime with dependencies.

    maybe(
        http_archive,
        name = "com_github_bytecodealliance_wasmtime",
        build_file = "@proxy_wasm_cpp_host//bazel/external:wasmtime.BUILD",
        sha256 = "2ccb49bb3bfa4d86907ad4c80d1147aef6156c7b6e3f7f14ed02a39de9761155",
        strip_prefix = "wasmtime-24.0.0",
        url = "https://github.com/bytecodealliance/wasmtime/archive/v24.0.0.tar.gz",
    )
