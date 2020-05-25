# Copyright 2020 Istio Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
################################################################################
#

load("@rules_proto//proto:defs.bzl", "proto_library")
load("@rules_cc//cc:defs.bzl", "cc_proto_library")

def _wasm_transition_impl(settings, attr):
    return {
        "//command_line_option:cpu": "wasm",
        "//command_line_option:crosstool_top": "@proxy_wasm_cpp_sdk//toolchain:emscripten",

        # Overriding copt/cxxopt/linkopt to prevent sanitizers/coverage options leak
        # into WASM build configuration
        "//command_line_option:copt": [],
        "//command_line_option:cxxopt": [],
        "//command_line_option:linkopt": [],
    }

def _cc_library_wasm_transition_impl(settings, attr):
    return {
        # Overriding copt/cxxopt/linkopt to prevent sanitizers/coverage options leak
        # into WASM build configuration
        "//command_line_option:copt": [],
        "//command_line_option:cxxopt": ["-std=c++17"],
        "//command_line_option:linkopt": [], 
    }

wasm_transition = transition(
    implementation = _wasm_transition_impl,
    inputs = [],
    outputs = [
        "//command_line_option:cpu",
        "//command_line_option:crosstool_top",
        "//command_line_option:copt",
        "//command_line_option:cxxopt",
        "//command_line_option:linkopt",
    ],
)

cc_library_wasm_transition = transition(
    implementation = _cc_library_wasm_transition_impl,
    inputs = [],
    outputs = [
        "//command_line_option:copt",
        "//command_line_option:cxxopt",
        "//command_line_option:linkopt", 
    ]
)

def _wasm_binary_impl(ctx):
    out = ctx.actions.declare_file(ctx.label.name)
    ctx.actions.run_shell(
        command = 'cp "{}" "{}"'.format(ctx.files.binary[0].path, out.path),
        outputs = [out],
        inputs = ctx.files.binary,
    )

    return [DefaultInfo(runfiles = ctx.runfiles([out]))]

# WASM binary rule implementation.
# This copies the binary specified in binary attribute in WASM configuration to
# target configuration, so a binary in non-WASM configuration can depend on them.
wasm_binary = rule(
    implementation = _wasm_binary_impl,
    attrs = {
        "binary": attr.label(mandatory = True, cfg = wasm_transition),
        "_whitelist_function_transition": attr.label(default = "@bazel_tools//tools/whitelists/function_transition_whitelist"),
    },
)

def _wasm_library_impl(ctx):
    return [deps[CcInfo] for deps in ctx.attr.deps]

wasm_library = rule(
    implementation = _wasm_library_impl,
    attrs = {
        "deps": attr.label(mandatory = True, providers = [CcInfo, DefaultInfo], cfg = cc_library_wasm_transition),
        "_cc_toolchain": attr.label(default = Label("@bazel_tools//tools/cpp:current_cc_toolchain")),
        "_whitelist_function_transition": attr.label(default = "@bazel_tools//tools/whitelists/function_transition_whitelist"),
    },
    toolchains = ["@bazel_tools//tools/cpp:toolchain_type"],
)

def wasm_cc_binary(name, **kwargs):
    wasm_name = "_wasm_" + name
    kwargs.setdefault("additional_linker_inputs", ["@proxy_wasm_cpp_sdk//:jslib"])
    kwargs.setdefault("linkopts", ["--js-library external/proxy_wasm_cpp_sdk/proxy_wasm_intrinsics.js"])
    kwargs.setdefault("visibility", ["//visibility:public"])
    native.cc_binary(
        name = wasm_name,
        # Adding manual tag it won't be built in non-WASM (e.g. x86_64 config)
        # when an wildcard is specified, but it will be built in WASM configuration
        # when the wasm_binary below is built.
        tags = ["manual"],
        **kwargs
    )

    wasm_binary(
        name = name,
        binary = ":" + wasm_name,
    )

def wasm_cc_library(name, **kwargs):
    lib_name = "_lib_" + name
    native.cc_library(
        name = lib_name,
        **kwargs
    )

    wasm_library(
        name = name,
        deps = ":" + lib_name
    )

def wasm_cc_proto_library(name, deps):
    proto_name = "_proto_" + name
    
    cc_proto_library(
        name = proto_name,
        deps = [deps],
    )

    wasm_library(
        name = name,
        deps = ":" + proto_name
    )