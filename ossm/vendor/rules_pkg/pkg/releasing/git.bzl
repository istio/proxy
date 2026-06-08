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
"""A rule to extract the git changelog."""

def _git_changelog_impl(ctx):
    """Implements to git_changelog rule."""

    args = ctx.actions.args()
    tools = []

    toolchain = ctx.toolchains["@rules_pkg//toolchains/git:git_toolchain_type"].git
    if not toolchain.valid:
        fail("The git_toolchain is not properly configured: " +
             toolchain.name)
    if toolchain.path:
        args.add("--git_path", toolchain.path)
    else:
        executable = toolchain.label[DefaultInfo].files_to_run.executable
        tools.append(executable)
        tools.append(toolchain.label[DefaultInfo].default_runfiles.files.to_list())
        args.add("--git_path", executable.path)
    args.add("--git_root", toolchain.client_top)
    args.add("--from_ref", ctx.attr.from_ref)
    args.add("--to_ref", ctx.attr.to_ref)
    args.add("--out", ctx.outputs.out.path)
    if ctx.attr.verbose:
        args.add("--verbose")

    ctx.actions.run(
        mnemonic = "GitChangelog",
        executable = ctx.executable._git_changelog,
        use_default_shell_env = True,
        arguments = [args],
        outputs = [ctx.outputs.out],
        env = {
            "LANG": "en_US.UTF-8",
            "LC_CTYPE": "UTF-8",
            "PYTHONIOENCODING": "UTF-8",
            "PYTHONUTF8": "1",
        },
        execution_requirements = {
            "local": "1",
        },
        tools = tools,
    )

# Define the rule.
_git_changelog = rule(
    doc = "Extracts the git changelog between two refs.",
    attrs = {
        "from_ref": attr.string(
            doc = "lower commit ref. The default is to use the latest tag",
            default = "_LATEST_TAG_",
        ),
        "to_ref": attr.string(
            doc = "upper commit ref. The default is HEAD",
            default = "HEAD",
        ),
        "out": attr.output(mandatory = True),
        "verbose": attr.bool(
            doc = "Be verbose",
            default = False,
        ),
        "_git_changelog": attr.label(
            default = Label("//pkg/releasing:git_changelog_private"),
            cfg = "exec",
            executable = True,
            allow_files = True,
        ),
    },
    implementation = _git_changelog_impl,
    toolchains = ["@rules_pkg//toolchains/git:git_toolchain_type"],
)

def git_changelog(name, **kwargs):
    _git_changelog(
        name = name,
        # This requires bazel 4.x
        target_compatible_with = select({
            # Force label resolution to be rules_pkg, instead of my repo.
            str(Label("//toolchains/git:have_git")): [],
            "//conditions:default": ["//:not_compatible"],
        }),
        **kwargs
    )
