# Copyright 2021 The Bazel Authors. All rights reserved.
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
"""toolchain to provide access to git from Bazel.

A git toolchain is somewhat unusual compared to other toolchains.
It not only provides a path to the git executable, but it also
holds the absolute path to our workspace, which is presumed to
be under revision control. This allows us to write helper tools
that escape the Bazel sandbox and pop back to the workspace/repo.
"""

GitInfo = provider(
    doc = """Information needed to invoke git.""",
    fields = {
        "name": "The name of the toolchain",
        "valid": "Is this toolchain valid and usable?",
        "label": "Label of a target providing a git binary",
        "path": "The path to a pre-built git",
        "client_top": "The path to the top of the git client." +
                      " In reality, we use the path to the WORKSPACE file as" +
                      " a proxy for a folder underneath the git client top.",
    },
)

def _git_toolchain_impl(ctx):
    if ctx.attr.label and ctx.attr.path:
        fail("git_toolchain must not specify both label and path.")
    valid = bool(ctx.attr.label) or bool(ctx.attr.path)
    toolchain_info = platform_common.ToolchainInfo(
        git = GitInfo(
            name = str(ctx.label),
            valid = valid,
            label = ctx.attr.label,
            path = ctx.attr.path,
            client_top = ctx.attr.client_top,
        ),
    )
    return [toolchain_info]

git_toolchain = rule(
    implementation = _git_toolchain_impl,
    attrs = {
        "label": attr.label(
            doc = "A valid label of a target to build or a prebuilt binary. Mutually exclusive with path.",
            cfg = "exec",
            executable = True,
            allow_files = True,
        ),
        "path": attr.string(
            doc = "The path to the git executable. Mutually exclusive with label.",
        ),
        "client_top": attr.string(
            doc = "The top of your git client.",
        ),
    },
)

# Expose the presence of a git in the resolved toolchain as a flag.
def _is_git_available_impl(ctx):
    toolchain = ctx.toolchains["@rules_pkg//toolchains/git:git_toolchain_type"]
    available = toolchain and toolchain.git.valid
    return [config_common.FeatureFlagInfo(
        value = ("1" if available else "0"),
    )]

is_git_available = rule(
    implementation = _is_git_available_impl,
    attrs = {},
    toolchains = ["@rules_pkg//toolchains/git:git_toolchain_type"],
)
