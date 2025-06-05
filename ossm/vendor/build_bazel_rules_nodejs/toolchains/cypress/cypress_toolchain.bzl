# Copyright 2018 The Bazel Authors. All rights reserved.
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

"""This module implements the cypress toolchain rule.
"""

CypressInfo = provider(
    doc = "Information about how to invoke the cypress executable.",
    fields = {
        "cypress_bin": "Target for the bazel installed cypress executable for the target platform.",
        "cypress_bin_path": "Path to the system installed cypress executable for the target platform.",
        "cypress_files": """Files required in runfiles to make the cypress executable available.

May be empty if the cypress_bin_path points to a locally installed cypress binary.""",
    },
)

def _cypress_toolchain_impl(ctx):
    if ctx.attr.cypress_bin and ctx.attr.cypress_bin_path:
        fail("Can only set one of cypress_bin or cypress_bin_path but both were set.")
    if not ctx.attr.cypress_bin and not ctx.attr.cypress_bin_path:
        fail("Must set one of cypress_bin or cypress_bin_path.")
    if ctx.attr.cypress_bin and not ctx.attr.cypress_files:
        fail("Must set cypress_files when cypress_bin is set.")

    cypress_files = []
    cypress_bin_path = ctx.attr.cypress_bin_path

    if ctx.attr.cypress_bin:
        cypress_files = ctx.attr.cypress_bin.files.to_list() + ctx.attr.cypress_files.files.to_list()
        cypress_bin_path = cypress_files[0].short_path

    return [
        platform_common.ToolchainInfo(
            cypressinfo = CypressInfo(
                cypress_bin_path = cypress_bin_path,
                cypress_files = cypress_files,
            ),
        ),
    ]

cypress_toolchain = rule(
    implementation = _cypress_toolchain_impl,
    attrs = {
        "cypress_bin": attr.label(
            doc = "A hermetically downloaded cypress executable binary for the target platform.",
            mandatory = False,
            allow_single_file = True,
        ),
        "cypress_bin_path": attr.string(
            doc = "Path to an existing cypress executable for the target platform.",
            mandatory = False,
        ),
        "cypress_files": attr.label(
            doc = "A hermetically downloaded cypress filegroup of all cypress binary files for the target platform. Must be set when cypress_bin is set.",
            mandatory = False,
            allow_files = True,
        ),
    },
    doc = """Defines a cypress toolchain.

For usage see https://docs.bazel.build/versions/main/toolchains.html#defining-toolchains.
""",
)
