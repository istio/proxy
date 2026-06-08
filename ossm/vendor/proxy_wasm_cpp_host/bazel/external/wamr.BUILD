# Copyright 2025 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

load("@rules_cc//cc:defs.bzl", "cc_library")
load("@rules_foreign_cc//foreign_cc:defs.bzl", "cmake")

licenses(["notice"])  # Apache 2

package(default_visibility = ["//visibility:public"])

filegroup(
    name = "srcs",
    srcs = glob(["**"]),
)

cmake(
    name = "wamr_lib_cmake",
    cache_entries = select({
        "@proxy_wasm_cpp_host//bazel:engine_wamr_jit": {
            "BAZEL_BUILD": "ON",
            # Set LLVM_INCLUDE_DIR for the patch to use
            "LLVM_INCLUDE_DIR": "$$EXT_BUILD_ROOT/external/llvm_toolchain_llvm/include",
        },
        "//conditions:default": {},
    }),
    # LLVM dependencies for JIT are provided via Bazel, not CMake
    # The patch skips LLVM CMake detection when BAZEL_BUILD is set
    # LLVM headers from hermetic toolchain (bzlmod-compatible via data attribute)
    # LLVM libraries are linked via cc_library deps (see wamr_lib below)
    data = select({
        "@proxy_wasm_cpp_host//bazel:engine_wamr_jit": [
            "@llvm_toolchain_llvm//:all_includes",
        ],
        "//conditions:default": [],
    }),
    env = select({
        "@proxy_wasm_cpp_host//bazel:engine_wamr_jit": {
            # Reference LLVM headers in sandbox via EXT_BUILD_ROOT
            # The data attribute ensures llvm_toolchain_llvm is mounted in sandbox
            # This path works with both WORKSPACE and bzlmod
            "CFLAGS": "-isystem $$EXT_BUILD_ROOT/external/llvm_toolchain_llvm/include",
            "CXXFLAGS": "-isystem $$EXT_BUILD_ROOT/external/llvm_toolchain_llvm/include",
        },
        "//conditions:default": {},
    }),
    generate_args = [
        # disable WASI
        "-DWAMR_BUILD_LIBC_WASI=0",
        "-DWAMR_BUILD_LIBC_BUILTIN=0",
        # MVP
        "-DWAMR_BUILD_BULK_MEMORY=1",
        "-DWAMR_BUILD_REF_TYPES=1",
        "-DWAMR_BUILD_TAIL_CALL=1",
        # WAMR private features
        "-DWAMR_BUILD_MULTI_MODULE=0",
        # Some tests have indicated that the following three factors have
        #   a minimal impact on performance.
        # - Get function names from name section
        "-DWAMR_BUILD_CUSTOM_NAME_SECTION=1",
        "-DWAMR_BUILD_LOAD_CUSTOM_SECTION=1",
        # - Show Wasm call stack if met a trap
        "-DWAMR_BUILD_DUMP_CALL_STACK=1",
        # Cache module files
        "-DWAMR_BUILD_WASM_CACHE=0",
        "-GNinja",
    ] + select({
        "@proxy_wasm_cpp_host//bazel:engine_wamr_jit": [
            # WAMR's CMake will find LLVM via CMAKE_PREFIX_PATH
            # No need to set LLVM_DIR explicitly
            "-DWAMR_BUILD_AOT=1",
            "-DWAMR_BUILD_FAST_INTERP=0",
            "-DWAMR_BUILD_INTERP=0",
            "-DWAMR_BUILD_JIT=1",
            "-DWAMR_BUILD_SIMD=1",
            # linux perf. only for jit and aot
            # "-DWAMR_BUILD_LINUX_PERF=1",
        ],
        "//conditions:default": [
            "-DWAMR_BUILD_AOT=0",
            "-DWAMR_BUILD_FAST_INTERP=1",
            "-DWAMR_BUILD_INTERP=1",
            "-DWAMR_BUILD_JIT=0",
            "-DWAMR_BUILD_SIMD=0",
        ],
    }),
    lib_source = ":srcs",
    out_static_libs = ["libiwasm.a"],
)

# Wrapper library that adds LLVM dependencies for linking
cc_library(
    name = "wamr_lib",
    linkopts = select({
        "@proxy_wasm_cpp_host//bazel:engine_wamr_jit": ["-ldl"],
        "//conditions:default": [],
    }),
    deps = [":wamr_lib_cmake"] + select({
        "@proxy_wasm_cpp_host//bazel:engine_wamr_jit": [
            "@llvm-raw//:llvm_wamr_lib",
        ],
        "//conditions:default": [],
    }),
)
