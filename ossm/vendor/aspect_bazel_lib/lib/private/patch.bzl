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

"""Fork of @bazel_tools//tools/build_defs/repo:utils.bzl patch function with
working_directory argument added.

Upstream code is at
https://github.com/bazelbuild/bazel/blob/f4214746fcd15f0ef8c4e747ef8e3edca9f112a5/tools/build_defs/repo/utils.bzl#L87
"""

load(":repo_utils.bzl", "repo_utils")

# Temporary directory for downloading remote patch files.
_REMOTE_PATCH_DIR = ".tmp_remote_patches"

def _use_native_patch(patch_args):
    """If patch_args only contains -p<NUM> options, we can use the native patch implementation."""
    for arg in patch_args:
        if not arg.startswith("-p"):
            return False
    return True

def _download_patch(ctx, patch_url, integrity, auth):
    name = patch_url.split("/")[-1]
    patch_path = ctx.path(_REMOTE_PATCH_DIR).get_child(name)
    ctx.download(
        patch_url,
        patch_path,
        canonical_id = ctx.attr.canonical_id,
        auth = auth,
        integrity = integrity,
    )
    return patch_path

def patch(ctx, patches = None, patch_cmds = None, patch_cmds_win = None, patch_tool = None, patch_args = None, auth = None, patch_directory = None):
    """Implementation of patching an already extracted repository.

    This rule is intended to be used in the implementation function of
    a repository rule. If the parameters `patches`, `patch_tool`,
    `patch_args`, `patch_cmds` and `patch_cmds_win` are not specified
    then they are taken from `ctx.attr`.

    Args:
      ctx: The repository context of the repository rule calling this utility
        function.
      patches: The patch files to apply. List of strings, Labels, or paths.
      patch_cmds: Bash commands to run for patching, passed one at a
        time to bash -c. List of strings
      patch_cmds_win: Powershell commands to run for patching, passed
        one at a time to powershell /c. List of strings. If the
        boolean value of this parameter is false, patch_cmds will be
        used and this parameter will be ignored.
      patch_tool: Path of the patch tool to execute for applying
        patches. String.
      patch_args: Arguments to pass to the patch tool. List of strings.
      auth: An optional dict specifying authentication information for some of the URLs.
      patch_directory: Directory to apply the patches in

    """
    bash_exe = ctx.os.environ["BAZEL_SH"] if "BAZEL_SH" in ctx.os.environ else "bash"
    powershell_exe = ctx.os.environ["BAZEL_POWERSHELL"] if "BAZEL_POWERSHELL" in ctx.os.environ else "powershell.exe"

    if patches == None and hasattr(ctx.attr, "patches"):
        patches = ctx.attr.patches
    if patches == None:
        patches = []

    remote_patches = {}
    remote_patch_strip = 0
    if hasattr(ctx.attr, "remote_patches") and ctx.attr.remote_patches:
        if hasattr(ctx.attr, "remote_patch_strip"):
            remote_patch_strip = ctx.attr.remote_patch_strip
        remote_patches = ctx.attr.remote_patches

    if patch_cmds == None and hasattr(ctx.attr, "patch_cmds"):
        patch_cmds = ctx.attr.patch_cmds
    if patch_cmds == None:
        patch_cmds = []

    if patch_cmds_win == None and hasattr(ctx.attr, "patch_cmds_win"):
        patch_cmds_win = ctx.attr.patch_cmds_win
    if patch_cmds_win == None:
        patch_cmds_win = []

    if patch_tool == None and hasattr(ctx.attr, "patch_tool"):
        patch_tool = ctx.attr.patch_tool
    if not patch_tool:
        patch_tool = "patch"
        native_patch = True
    else:
        native_patch = False

    if patch_args == None and hasattr(ctx.attr, "patch_args"):
        patch_args = ctx.attr.patch_args
    if patch_args == None:
        patch_args = []

    if len(remote_patches) > 0 or len(patches) > 0 or len(patch_cmds) > 0:
        ctx.report_progress("Patching repository")

    # Apply remote patches
    for patch_url in remote_patches:
        integrity = remote_patches[patch_url]
        patchfile = _download_patch(ctx, patch_url, integrity, auth)
        ctx.patch(patchfile, remote_patch_strip)
        ctx.delete(patchfile)
    ctx.delete(ctx.path(_REMOTE_PATCH_DIR))

    # Apply local patches
    if native_patch and _use_native_patch(patch_args) and not patch_directory:
        if patch_args:
            strip = int(patch_args[-1][2:])
        else:
            strip = 0
        for patchfile in patches:
            ctx.patch(patchfile, strip)
    else:
        for patchfile in patches:
            command = "{patchtool} {patch_args} < {patchfile}".format(
                patchtool = patch_tool,
                patchfile = ctx.path(patchfile),
                patch_args = " ".join([
                    "'%s'" % arg
                    for arg in patch_args
                ]),
            )
            st = ctx.execute([bash_exe, "-c", command], working_directory = patch_directory)
            if st.return_code:
                msg = "Error applying patch {}:\n{}{}".format(str(patchfile), st.stderr, st.stdout)
                fail(msg)

    if repo_utils.is_windows(ctx) and patch_cmds_win:
        for cmd in patch_cmds_win:
            st = ctx.execute([powershell_exe, "/c", cmd], working_directory = patch_directory)
            if st.return_code:
                msg = "Error applying patch command {}:\n{}{}".format(cmd, st.stdout, st.stderr)
                fail(msg)
    else:
        for cmd in patch_cmds:
            st = ctx.execute([bash_exe, "-c", cmd], working_directory = patch_directory)
            if st.return_code:
                msg = "Error applying patch command {}:\n{}{}".format(cmd, st.stdout, st.stderr)
                fail(msg)
