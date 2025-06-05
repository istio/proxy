# Copyright 2022 Google LLC
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

load("@proxy_wasm_cpp_host//bazel:wasm.bzl", "wasm_rust_binary")
load("@proxy_wasm_cpp_sdk//bazel:defs.bzl", "proxy_wasm_cc_binary")

licenses(["notice"])  # Apache 2

package(default_visibility = ["//visibility:public"])

wasm_rust_binary(
    name = "abi_export.wasm",
    srcs = ["abi_export.rs"],
)

wasm_rust_binary(
    name = "abi_export.signed.with.key1.wasm",
    srcs = ["abi_export.rs"],
    signing_key = [
        "signature_key1.key",
        "signature_key1.pub",
    ],
)

wasm_rust_binary(
    name = "abi_export.signed.with.key2.wasm",
    srcs = ["abi_export.rs"],
    signing_key = [
        "signature_key2.key",
        "signature_key2.pub",
    ],
)

wasm_rust_binary(
    name = "bad_malloc.wasm",
    srcs = ["bad_malloc.rs"],
)

wasm_rust_binary(
    name = "callback.wasm",
    srcs = ["callback.rs"],
)

wasm_rust_binary(
    name = "trap.wasm",
    srcs = ["trap.rs"],
)

wasm_rust_binary(
    name = "resource_limits.wasm",
    srcs = ["resource_limits.rs"],
)

wasm_rust_binary(
    name = "env.wasm",
    srcs = ["env.rs"],
    wasi = True,
)

wasm_rust_binary(
    name = "clock.wasm",
    srcs = ["clock.rs"],
    wasi = True,
)

wasm_rust_binary(
    name = "random.wasm",
    srcs = ["random.rs"],
    wasi = True,
)

proxy_wasm_cc_binary(
    name = "canary_check.wasm",
    srcs = ["canary_check.cc"],
)

proxy_wasm_cc_binary(
    name = "http_logging.wasm",
    srcs = ["http_logging.cc"],
)
