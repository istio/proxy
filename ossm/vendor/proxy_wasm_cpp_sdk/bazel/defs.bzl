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

load("@emsdk//emscripten_toolchain:wasm_rules.bzl", "wasm_cc_binary")
load("@rules_cc//cc:defs.bzl", "cc_binary")

def _optimized_wasm_cc_binary_transition_impl(settings, attr):
    # Define STANDALONE_WASM at compile time as well as link time (below).
    # This fixes Abseil by not including Emscripten JS stacktraces + symbolization.
    # TODO(martijneken): Remove after Abseil stops using this define.
    return {
        "//command_line_option:copt": ["-O3", "-flto", "-DSTANDALONE_WASM"],
        "//command_line_option:cxxopt": [],
        "//command_line_option:linkopt": [],
        # LTO is not compatible with fission (split DWARF), so disable fission.
        "//command_line_option:fission": "no",
        "//command_line_option:collect_code_coverage": False,
    }

_optimized_wasm_cc_binary_transition = transition(
    implementation = _optimized_wasm_cc_binary_transition_impl,
    inputs = [],
    outputs = [
        "//command_line_option:copt",
        "//command_line_option:cxxopt",
        "//command_line_option:linkopt",
        "//command_line_option:fission",
        "//command_line_option:collect_code_coverage",
    ],
)

def _optimized_wasm_cc_binary_impl(ctx):
    input_binary = ctx.attr.wasm_cc_target[0][DefaultInfo].files_to_run.executable
    input_runfiles = ctx.attr.wasm_cc_target[0][DefaultInfo].default_runfiles
    copied_binary = ctx.actions.declare_file(ctx.attr.name)

    ctx.actions.run(
        mnemonic = "CopyFile",
        executable = "cp",
        arguments = [input_binary.path, copied_binary.path],
        inputs = [input_binary],
        outputs = [copied_binary],
    )

    return DefaultInfo(
        executable = copied_binary,
        runfiles = input_runfiles,
    )

_optimized_wasm_cc_binary = rule(
    implementation = _optimized_wasm_cc_binary_impl,
    attrs = {
        "wasm_cc_target": attr.label(
            doc = "The wasm_cc_binary to extract files from.",
            cfg = _optimized_wasm_cc_binary_transition,
            mandatory = True,
        ),
        "_allowlist_function_transition": attr.label(
            default = "@bazel_tools//tools/allowlists/function_transition_allowlist",
        ),
    },
    executable = True,
)

def proxy_wasm_cc_binary(
        name,
        additional_linker_inputs = [],
        linkopts = [],
        tags = [],
        deps = [],
        protobuf = "",
        **kwargs):
    proxy_wasm_deps = ["@proxy_wasm_cpp_sdk//:proxy_wasm_intrinsics"]
    if protobuf == "lite":
        proxy_wasm_deps.append("@proxy_wasm_cpp_sdk//:proxy_wasm_intrinsics_lite")
    if protobuf == "full":
        proxy_wasm_deps.append("@proxy_wasm_cpp_sdk//:proxy_wasm_intrinsics_full")

    cc_binary(
        name = "proxy_wasm_" + name.rstrip(".wasm"),
        additional_linker_inputs = additional_linker_inputs + [
            "@proxy_wasm_cpp_sdk//:proxy_wasm_intrinsics_js",
        ],
        linkopts = linkopts + [
            # Setting to indicate module is a "reactor library" without a main() entry point:
            # https://emscripten.org/docs/tools_reference/settings_reference.html#standalone-wasm
            "--no-entry",
            # File listing additional functions that Emscripten should expect to be implemented by the host:
            # https://emscripten.org/docs/porting/connecting_cpp_and_javascript/Interacting-with-code.html#implement-c-in-javascript
            "--js-library=$(location @proxy_wasm_cpp_sdk//:proxy_wasm_intrinsics_js)",
            # Emit Wasm module that can run without JavaScript
            "-sSTANDALONE_WASM",
            # Give host code access to Emscripten's _malloc() function
            "-sEXPORTED_FUNCTIONS=_malloc",
            # Allow allocating memory past initial heap size
            "-sALLOW_MEMORY_GROWTH=1",
        ],
        tags = tags + [
            "manual",
        ],
        deps = deps + proxy_wasm_deps,
        **kwargs
    )

    wasm_cc_binary(
        standalone = True,
        threads = "off",
        name = "wasm_" + name,
        cc_target = ":proxy_wasm_" + name.rstrip(".wasm"),
        tags = tags + [
            "manual",
        ],
    )

    _optimized_wasm_cc_binary(
        name = name,
        wasm_cc_target = ":wasm_" + name,
        tags = tags,
    )
