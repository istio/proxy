# Copyright 2024 The Bazel Authors. All rights reserved.
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

"""
EXPERIMENTAL: This is experimental and may be removed without notice

This module implements the uv toolchain rule
"""

load(":uv_toolchain_info.bzl", "UvToolchainInfo")

def _uv_toolchain_impl(ctx):
    uv = ctx.attr.uv

    default_info = DefaultInfo(
        files = uv.files,
        runfiles = uv[DefaultInfo].default_runfiles,
    )
    uv_toolchain_info = UvToolchainInfo(
        uv = uv,
        version = ctx.attr.version,
    )

    # Export all the providers inside our ToolchainInfo
    # so the current_toolchain rule can grab and re-export them.
    toolchain_info = platform_common.ToolchainInfo(
        default_info = default_info,
        uv_toolchain_info = uv_toolchain_info,
    )
    return [
        default_info,
        toolchain_info,
    ]

uv_toolchain = rule(
    implementation = _uv_toolchain_impl,
    attrs = {
        "uv": attr.label(
            doc = "A static uv binary.",
            mandatory = True,
            allow_single_file = True,
            executable = True,
            cfg = "target",
        ),
        "version": attr.string(mandatory = True, doc = "Version of the uv binary."),
    },
    doc = "Defines a uv toolchain.",
)
