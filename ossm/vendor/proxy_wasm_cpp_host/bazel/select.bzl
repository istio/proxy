# Copyright 2021 Google LLC
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

def proxy_wasm_select_engine_null(xs):
    return select({
        "@proxy_wasm_cpp_host//bazel:engine_null": xs,
        "@proxy_wasm_cpp_host//bazel:multiengine": xs,
        "//conditions:default": [],
    })

def proxy_wasm_select_engine_v8(xs):
    return select({
        "@proxy_wasm_cpp_host//bazel:engine_v8": xs,
        "@proxy_wasm_cpp_host//bazel:multiengine": xs,
        "//conditions:default": [],
    })

def proxy_wasm_select_engine_wamr(xs):
    return select({
        "@proxy_wasm_cpp_host//bazel:engine_wamr_interp": xs,
        "@proxy_wasm_cpp_host//bazel:engine_wamr_jit": xs,
        "@proxy_wasm_cpp_host//bazel:multiengine": xs,
        "//conditions:default": [],
    })

def proxy_wasm_select_engine_wasmtime(xs, xp):
    return select({
        "@proxy_wasm_cpp_host//bazel:engine_wasmtime": xs,
        "@proxy_wasm_cpp_host//bazel:multiengine": xp,
        "//conditions:default": [],
    })

def proxy_wasm_select_engine_wasmedge(xs):
    return select({
        "@proxy_wasm_cpp_host//bazel:engine_wasmedge": xs,
        "@proxy_wasm_cpp_host//bazel:multiengine": xs,
        "//conditions:default": [],
    })

def proxy_wasm_select_engine_wavm(xs):
    return select({
        "@proxy_wasm_cpp_host//bazel:engine_wavm": xs,
        "@proxy_wasm_cpp_host//bazel:multiengine": xs,
        "//conditions:default": [],
    })
