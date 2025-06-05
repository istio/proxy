# Copyright 2023 The Bazel Authors. All rights reserved.
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
"""Implementation of cc_tool"""

load("@bazel_skylib//rules/directory:providers.bzl", "DirectoryInfo")
load("//cc/toolchains/impl:collect.bzl", "collect_data", "collect_provider")
load(
    ":cc_toolchain_info.bzl",
    "ToolCapabilityInfo",
    "ToolInfo",
)

def _cc_tool_impl(ctx):
    exe_info = ctx.attr.src[DefaultInfo]
    if exe_info.files_to_run != None and exe_info.files_to_run.executable != None:
        exe = exe_info.files_to_run.executable
    elif len(exe_info.files.to_list()) == 1:
        exe = exe_info.files.to_list()[0]
    else:
        fail("Expected cc_tool's src attribute to be either an executable or a single file")

    runfiles = collect_data(ctx, ctx.attr.data + [ctx.attr.src] + ctx.attr.allowlist_include_directories)
    tool = ToolInfo(
        label = ctx.label,
        exe = exe,
        runfiles = runfiles,
        execution_requirements = tuple(ctx.attr.tags),
        allowlist_include_directories = depset(
            direct = [d[DirectoryInfo] for d in ctx.attr.allowlist_include_directories],
        ),
        capabilities = tuple(collect_provider(ctx.attr.capabilities, ToolCapabilityInfo)),
    )

    link = ctx.actions.declare_file(ctx.label.name)
    ctx.actions.symlink(
        output = link,
        target_file = exe,
        is_executable = True,
    )
    return [
        tool,
        # This isn't required, but now we can do "bazel run <tool>", which can
        # be very helpful when debugging toolchains.
        DefaultInfo(
            files = depset([link]),
            runfiles = runfiles,
            executable = link,
        ),
    ]

cc_tool = rule(
    implementation = _cc_tool_impl,
    # @unsorted-dict-items
    attrs = {
        "src": attr.label(
            allow_files = True,
            cfg = "exec",
            doc = """The underlying binary that this tool represents.

Usually just a single prebuilt (eg. @toolchain//:bin/clang), but may be any
executable label.
""",
        ),
        "data": attr.label_list(
            allow_files = True,
            doc = """Additional files that are required for this tool to run.

Frequently, clang and gcc require additional files to execute as they often shell out to
other binaries (e.g. `cc1`).
""",
        ),
        "allowlist_include_directories": attr.label_list(
            providers = [DirectoryInfo],
            doc = """Include paths implied by using this tool.

Compilers may include a set of built-in headers that are implicitly available
unless flags like `-nostdinc` are provided. Bazel checks that all included
headers are properly provided by a dependency or allowlisted through this
mechanism.

As a rule of thumb, only use this if Bazel is complaining about absolute paths in your
toolchain and you've ensured that the toolchain is compiling with the `-no-canonical-prefixes`
and/or `-fno-canonical-system-headers` arguments.

This can help work around errors like:
`the source file 'main.c' includes the following non-builtin files with absolute paths
(if these are builtin files, make sure these paths are in your toolchain)`.
""",
        ),
        "capabilities": attr.label_list(
            providers = [ToolCapabilityInfo],
            doc = """Declares that a tool is capable of doing something.

For example, `@rules_cc//cc/toolchains/capabilities:supports_pic`.
""",
        ),
    },
    provides = [ToolInfo],
    doc = """Declares a tool for use by toolchain actions.

`cc_tool` rules are used in a `cc_tool_map` rule to ensure all files and
metadata required to run a tool are available when constructing a `cc_toolchain`.

In general, include all files that are always required to run a tool (e.g. libexec/** and
cross-referenced tools in bin/*) in the [data](#cc_tool-data) attribute. If some files are only
required when certain flags are passed to the tool, consider using a `cc_args` rule to
bind the files to the flags that require them. This reduces the overhead required to properly
enumerate a sandbox with all the files required to run a tool, and ensures that there isn't
unintentional leakage across configurations and actions.

Example:
```
load("//cc/toolchains:tool.bzl", "cc_tool")

cc_tool(
    name = "clang_tool",
    src = "@llvm_toolchain//:bin/clang",
    # Suppose clang needs libc to run.
    data = ["@llvm_toolchain//:lib/x86_64-linux-gnu/libc.so.6"]
    tags = ["requires-network"],
    capabilities = ["//cc/toolchains/capabilities:supports_pic"],
)
```
""",
    executable = True,
)
