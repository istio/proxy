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

licenses(["notice"])  # Apache 2

package(default_visibility = ["//visibility:public"])

# LLVM libraries needed by WAMR JIT - built with native Bazel.
# This replaces the foreign_cc cmake build of LLVM with native Bazel builds.
# These libraries are linked into the final binary, while WAMR's CMake build
# uses the hermetic LLVM toolchain's CMake configs for configuration only.
# Uses select() for CPU-specific libraries only.
cc_library(
    name = "llvm_wamr_lib",
    deps = [
        "@llvm-project//llvm:Analysis",
        "@llvm-project//llvm:BitReader",
        "@llvm-project//llvm:BitWriter",
        "@llvm-project//llvm:CodeGen",
        "@llvm-project//llvm:Core",
        "@llvm-project//llvm:ExecutionEngine",
        "@llvm-project//llvm:IPO",
        "@llvm-project//llvm:IRReader",
        "@llvm-project//llvm:InstCombine",
        "@llvm-project//llvm:Instrumentation",
        "@llvm-project//llvm:JITLink",
        "@llvm-project//llvm:Linker",
        "@llvm-project//llvm:MC",
        "@llvm-project//llvm:MCJIT",
        "@llvm-project//llvm:Object",
        "@llvm-project//llvm:OrcJIT",
        "@llvm-project//llvm:Passes",
        "@llvm-project//llvm:Scalar",
        "@llvm-project//llvm:Support",
        "@llvm-project//llvm:Target",
        "@llvm-project//llvm:TransformUtils",
        "@llvm-project//llvm:Vectorize",
    ] + select({
        "@platforms//cpu:x86_64": [
            "@llvm-project//llvm:X86AsmParser",
            "@llvm-project//llvm:X86CodeGen",
            "@llvm-project//llvm:X86Disassembler",
        ],
        "@platforms//cpu:aarch64": [
            "@llvm-project//llvm:AArch64AsmParser",
            "@llvm-project//llvm:AArch64CodeGen",
            "@llvm-project//llvm:AArch64Disassembler",
        ],
        "//conditions:default": [
            "@llvm-project//llvm:X86AsmParser",
            "@llvm-project//llvm:X86CodeGen",
            "@llvm-project//llvm:X86Disassembler",
        ],
    }),
)

# Create a tarball with LLVM headers preserving directory structure
# This is a robust, bzlmod-compatible solution for providing headers to rules_foreign_cc
genrule(
    name = "package_llvm_headers",
    srcs = ["@llvm_toolchain_llvm//:all_includes"],
    outs = ["llvm_headers.tar.gz"],
    cmd = """
        # Create temporary directory for building the archive
        TMPDIR=$$(mktemp -d)
        
        # Copy all headers preserving directory structure
        # The all_includes filegroup contains files like include/llvm/Config/llvm-config.h
        for src in $(SRCS); do
            # Extract the path relative to the workspace
            # Files are like external/llvm_toolchain_llvm/include/llvm/...
            rel_path=$$(echo $$src | sed 's|.*/llvm_toolchain_llvm/||')
            dest_path=$$TMPDIR/$$rel_path
            mkdir -p $$(dirname $$dest_path)
            cp $$src $$dest_path
        done
        
        # Create tarball from the temp directory
        tar -czf $(location llvm_headers.tar.gz) -C $$TMPDIR .
        rm -rf $$TMPDIR
    """,
)

filegroup(
    name = "llvm_headers",
    srcs = [":package_llvm_headers"],
)
