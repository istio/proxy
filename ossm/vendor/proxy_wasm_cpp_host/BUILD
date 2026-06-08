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

load(
    "@proxy_wasm_cpp_host//bazel:select.bzl",
    "proxy_wasm_select_engine_null",
    "proxy_wasm_select_engine_v8",
    "proxy_wasm_select_engine_wamr",
    "proxy_wasm_select_engine_wasmedge",
    "proxy_wasm_select_engine_wasmtime",
)
load("@rules_cc//cc:defs.bzl", "cc_library")

licenses(["notice"])  # Apache 2

package(default_visibility = ["//visibility:public"])

exports_files(["LICENSE"])

filegroup(
    name = "clang_tidy_config",
    data = [".clang-tidy"],
)

cc_library(
    name = "wasm_vm_headers",
    hdrs = [
        "include/proxy-wasm/limits.h",
        "include/proxy-wasm/sdk.h",
        "include/proxy-wasm/wasm_vm.h",
        "include/proxy-wasm/word.h",
    ],
    deps = [
        "@proxy_wasm_cpp_sdk//:common_lib",
    ],
)

cc_library(
    name = "headers",
    hdrs = [
        "include/proxy-wasm/context.h",
        "include/proxy-wasm/context_interface.h",
        "include/proxy-wasm/exports.h",
        "include/proxy-wasm/vm_id_handle.h",
        "include/proxy-wasm/wasm.h",
    ],
    deps = [
        ":wasm_vm_headers",
    ],
)

cc_library(
    name = "base_lib",
    srcs = [
        "src/bytecode_util.cc",
        "src/context.cc",
        "src/exports.cc",
        "src/hash.cc",
        "src/hash.h",
        "src/pairs_util.cc",
        "src/shared_data.cc",
        "src/shared_data.h",
        "src/shared_queue.cc",
        "src/shared_queue.h",
        "src/signature_util.cc",
        "src/vm_id_handle.cc",
        "src/wasm.cc",
        "src/wasm.h",
    ],
    hdrs = [
        "include/proxy-wasm/bytecode_util.h",
        "include/proxy-wasm/pairs_util.h",
        "include/proxy-wasm/signature_util.h",
    ],
    linkopts = select({
        "//bazel:crypto_system": ["-lcrypto"],
        "//conditions:default": [],
    }),
    deps = [
        ":headers",
    ] + select({
        "//bazel:crypto_system": [],
        "//conditions:default": ["@envoy//bazel:crypto"],
    }),
    alwayslink = 1,
)

cc_library(
    name = "null_lib",
    srcs = [
        "src/null/null.cc",
        "src/null/null_plugin.cc",
        "src/null/null_vm.cc",
    ],
    hdrs = [
        "include/proxy-wasm/null.h",
        "include/proxy-wasm/null_plugin.h",
        "include/proxy-wasm/null_vm.h",
        "include/proxy-wasm/null_vm_plugin.h",
        "include/proxy-wasm/wasm_api_impl.h",
    ],
    defines = [
        "PROXY_WASM_HAS_RUNTIME_NULL",
        "PROXY_WASM_HOST_ENGINE_NULL",
    ],
    deps = [
        ":headers",
        "@com_google_protobuf//:protobuf_lite",
        "@proxy_wasm_cpp_sdk//:api_lib",
    ],
)

cc_library(
    name = "v8_lib",
    srcs = [
        "src/v8/v8.cc",
    ],
    hdrs = ["include/proxy-wasm/v8.h"],
    defines = [
        "PROXY_WASM_HAS_RUNTIME_V8",
        "PROXY_WASM_HOST_ENGINE_V8",
    ],
    deps = [
        ":wasm_vm_headers",
        "@v8//:wee8",
    ],
)

cc_library(
    name = "wamr_lib",
    srcs = [
        "src/common/types.h",
        "src/wamr/types.h",
        "src/wamr/wamr.cc",
    ],
    hdrs = ["include/proxy-wasm/wamr.h"],
    defines = [
        "PROXY_WASM_HAS_RUNTIME_WAMR",
        "PROXY_WASM_HOST_ENGINE_WAMR",
    ],
    linkopts = select({
        "@platforms//os:macos": ["-Wl,-all_load"],
        "//conditions:default": [],
    }),
    deps = [
        ":wasm_vm_headers",
        "@envoy//bazel/foreign_cc:wamr",
    ],
)

cc_library(
    name = "wasmedge_lib",
    srcs = [
        "src/common/types.h",
        "src/wasmedge/types.h",
        "src/wasmedge/wasmedge.cc",
    ],
    hdrs = ["include/proxy-wasm/wasmedge.h"],
    defines = [
        "PROXY_WASM_HAS_RUNTIME_WASMEDGE",
        "PROXY_WASM_HOST_ENGINE_WASMEDGE",
    ],
    linkopts = select({
        "@platforms//os:macos": [
            "-ldl",
        ],
        "//conditions:default": [
            "-lrt",
            "-ldl",
        ],
    }),
    deps = [
        ":wasm_vm_headers",
        "@com_github_wasmedge_wasmedge//:wasmedge_lib",
    ],
)

cc_library(
    name = "wasmtime_lib",
    srcs = [
        "src/common/types.h",
        "src/wasmtime/types.h",
        "src/wasmtime/wasmtime.cc",
    ],
    hdrs = ["include/proxy-wasm/wasmtime.h"],
    copts = [
        "-DWASM_API_EXTERN=",
    ],
    defines = [
        "PROXY_WASM_HAS_RUNTIME_WASMTIME",
        "PROXY_WASM_HOST_ENGINE_WASMTIME",
    ],
    # See: https://bytecodealliance.github.io/wasmtime/c-api/
    linkopts = select({
        "@platforms//os:macos": [],
        "@platforms//os:windows": [
            "ws2_32.lib",
            "advapi32.lib",
            "userenv.lib",
            "ntdll.lib",
            "shell32.lib",
            "ole32.lib",
            "bcrypt.lib",
        ],
        "//conditions:default": [
            "-ldl",
            "-lm",
            "-lpthread",
        ],
    }),
    deps = [
        ":wasm_vm_headers",
        "@wasmtime//:wasmtime_lib",
    ],
)

genrule(
    name = "prefixed_wasmtime_sources",
    srcs = [
        "src/wasmtime/types.h",
        "src/wasmtime/wasmtime.cc",
    ],
    outs = [
        "src/wasmtime/prefixed_types.h",
        "src/wasmtime/prefixed_wasmtime.cc",
    ],
    cmd = """
        for file in $(SRCS); do
           sed -e 's/wasm_/wasmtime_wasm_/g' \
               -e 's/include\\/wasm.h/include\\/prefixed_wasm.h/g' \
               -e 's/wasmtime\\/types.h/wasmtime\\/prefixed_types.h/g' \
           $$file >$(@D)/$$(dirname $$file)/prefixed_$$(basename $$file)
        done
        """,
)

cc_library(
    name = "prefixed_wasmtime_lib",
    srcs = [
        "src/common/types.h",
        "src/wasmtime/prefixed_types.h",
        "src/wasmtime/prefixed_wasmtime.cc",
    ],
    hdrs = ["include/proxy-wasm/wasmtime.h"],
    copts = [
        "-DWASM_API_EXTERN=",
    ],
    defines = [
        "PROXY_WASM_HAS_RUNTIME_WASMTIME",
        "PROXY_WASM_HOST_ENGINE_WASMTIME",
    ],
    # See: https://bytecodealliance.github.io/wasmtime/c-api/
    linkopts = select({
        "@platforms//os:macos": [],
        "@platforms//os:windows": [
            "ws2_32.lib",
            "advapi32.lib",
            "userenv.lib",
            "ntdll.lib",
            "shell32.lib",
            "ole32.lib",
            "bcrypt.lib",
        ],
        "//conditions:default": [
            "-ldl",
            "-lm",
            "-lpthread",
        ],
    }),
    deps = [
        ":wasm_vm_headers",
        "@wasmtime//:prefixed_wasmtime_lib",
    ],
)

cc_library(
    name = "lib",
    deps = [
        ":base_lib",
    ] + proxy_wasm_select_engine_null(
        [":null_lib"],
    ) + proxy_wasm_select_engine_v8(
        [":v8_lib"],
    ) + proxy_wasm_select_engine_wamr(
        [":wamr_lib"],
    ) + proxy_wasm_select_engine_wasmedge(
        [":wasmedge_lib"],
    ) + proxy_wasm_select_engine_wasmtime(
        [":wasmtime_lib"],
        [":prefixed_wasmtime_lib"],
    ),
)
