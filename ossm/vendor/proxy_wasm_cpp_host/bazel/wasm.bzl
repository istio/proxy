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

load("@rules_rust//rust:defs.bzl", "rust_binary")

def _wasm_rust_transition_impl(settings, attr):
    return {
        "//command_line_option:platforms": "@rules_rust//rust/platform:wasm",
    }

def _wasi_rust_transition_impl(settings, attr):
    return {
        "//command_line_option:platforms": "@rules_rust//rust/platform:wasi",
    }

wasm_rust_transition = transition(
    implementation = _wasm_rust_transition_impl,
    inputs = [],
    outputs = [
        "//command_line_option:platforms",
    ],
)

wasi_rust_transition = transition(
    implementation = _wasi_rust_transition_impl,
    inputs = [],
    outputs = [
        "//command_line_option:platforms",
    ],
)

def _wasm_binary_impl(ctx):
    out = ctx.actions.declare_file(ctx.label.name)
    if ctx.attr.signing_key:
        ctx.actions.run(
            executable = ctx.executable._wasmsign_tool,
            arguments = ["--sign", "--use-custom-section", "--sk-path", ctx.files.signing_key[0].path, "--pk-path", ctx.files.signing_key[1].path, "--input", ctx.files.binary[0].path, "--output", out.path],
            outputs = [out],
            inputs = ctx.files.binary + ctx.files.signing_key,
        )
    else:
        ctx.actions.run(
            executable = "cp",
            arguments = [ctx.files.binary[0].path, out.path],
            outputs = [out],
            inputs = ctx.files.binary,
        )

    return [DefaultInfo(files = depset([out]), runfiles = ctx.runfiles([out]))]

def _wasm_attrs(transition):
    return {
        "binary": attr.label(mandatory = True, cfg = transition),
        "signing_key": attr.label_list(allow_files = True),
        "_wasmsign_tool": attr.label(default = "//bazel/cargo/wasmsign/remote:wasmsign__wasmsign", executable = True, cfg = "exec"),
        "_whitelist_function_transition": attr.label(default = "@bazel_tools//tools/whitelists/function_transition_whitelist"),
    }

wasm_rust_binary_rule = rule(
    implementation = _wasm_binary_impl,
    attrs = _wasm_attrs(wasm_rust_transition),
)

wasi_rust_binary_rule = rule(
    implementation = _wasm_binary_impl,
    attrs = _wasm_attrs(wasi_rust_transition),
)

def wasm_rust_binary(name, tags = [], wasi = False, signing_key = [], **kwargs):
    wasm_name = "_wasm_" + name.replace(".", "_")
    kwargs.setdefault("visibility", ["//visibility:public"])

    rust_binary(
        name = wasm_name,
        edition = "2018",
        crate_type = "cdylib",
        out_binary = True,
        tags = ["manual"],
        **kwargs
    )

    bin_rule = wasm_rust_binary_rule
    if wasi:
        bin_rule = wasi_rust_binary_rule

    bin_rule(
        name = name,
        binary = ":" + wasm_name,
        signing_key = signing_key,
        tags = tags + ["manual"],
    )
