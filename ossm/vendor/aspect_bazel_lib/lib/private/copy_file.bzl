# Copyright 2019 The Bazel Authors. All rights reserved.
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

"""Implementation of copy_file macro and underlying rules.

These rules copy a file to another location using hermetic uutils/coreutils `cp`.
`_copy_xfile` marks the resulting file executable, `_copy_file` does not.
"""

load(":copy_common.bzl", _COPY_EXECUTION_REQUIREMENTS = "COPY_EXECUTION_REQUIREMENTS")
load(":directory_path.bzl", "DirectoryPathInfo")

_COREUTILS_TOOLCHAIN = "@aspect_bazel_lib//lib:coreutils_toolchain_type"

# Declare toolchains used by copy file actions so that downstream rulesets can pass it into
# the `toolchains` attribute of their rule.
COPY_FILE_TOOLCHAINS = [
    _COREUTILS_TOOLCHAIN,
]

def copy_file_action(ctx, src, dst, dir_path = None):
    """Factory function that creates an action to copy a file from src to dst.

    If src is a TreeArtifact, dir_path must be specified as the path within
    the TreeArtifact to the file to copy.

    This helper is used by copy_file. It is exposed as a public API so it can be used within
    other rule implementations.

    To use `copy_file_action` in your own rules, you need to include the toolchains it uses
    in your rule definition. For example:

    ```starlark
    load("@aspect_bazel_lib//lib:copy_file.bzl", "COPY_FILE_TOOLCHAINS")

    my_rule = rule(
        ...,
        toolchains = COPY_FILE_TOOLCHAINS,
    )
    ```

    Additionally, you must ensure that the coreutils toolchain is has been registered in your
    WORKSPACE if you are not using bzlmod:

    ```starlark
    load("@aspect_bazel_lib//lib:repositories.bzl", "register_coreutils_toolchains")

    register_coreutils_toolchains()
    ```

    Args:
        ctx: The rule context.
        src: The source file to copy or TreeArtifact to copy a single file out of.
        dst: The destination file.
        dir_path: If src is a TreeArtifact, the path within the TreeArtifact to the file to copy.
    """

    if dst.is_directory:
        fail("dst must not be a TreeArtifact")
    if src.is_directory:
        if not dir_path:
            fail("dir_path must be set if src is a TreeArtifact")
        src_path = "/".join([src.path, dir_path])
    else:
        src_path = src.path

    coreutils = ctx.toolchains[_COREUTILS_TOOLCHAIN].coreutils_info

    ctx.actions.run(
        executable = coreutils.bin,
        arguments = ["cp", src_path, dst.path],
        inputs = [src],
        outputs = [dst],
        mnemonic = "CopyFile",
        progress_message = "Copying file %{input}",
        execution_requirements = _COPY_EXECUTION_REQUIREMENTS,
        toolchain = "@aspect_bazel_lib//lib:coreutils_toolchain_type",
    )

def _copy_file_impl(ctx):
    if ctx.attr.allow_symlink:
        if len(ctx.files.src) != 1:
            fail("src must be a single file when allow_symlink is True")
        if ctx.files.src[0].is_directory:
            fail("cannot use copy_file to create a symlink to a directory")
        ctx.actions.symlink(
            output = ctx.outputs.out,
            target_file = ctx.files.src[0],
            is_executable = ctx.attr.is_executable,
        )
    elif DirectoryPathInfo in ctx.attr.src:
        copy_file_action(
            ctx,
            ctx.attr.src[DirectoryPathInfo].directory,
            ctx.outputs.out,
            dir_path = ctx.attr.src[DirectoryPathInfo].path,
        )
    else:
        if len(ctx.files.src) != 1:
            fail("src must be a single file or a target that provides a DirectoryPathInfo")
        if ctx.files.src[0].is_directory:
            fail("cannot use copy_file on a directory; try copy_directory instead")
        copy_file_action(ctx, ctx.files.src[0], ctx.outputs.out)

    files = depset(direct = [ctx.outputs.out])
    runfiles = ctx.runfiles(files = [ctx.outputs.out])
    if ctx.attr.is_executable:
        return [DefaultInfo(files = files, runfiles = runfiles, executable = ctx.outputs.out)]
    else:
        return [DefaultInfo(files = files, runfiles = runfiles)]

_ATTRS = {
    "src": attr.label(mandatory = True, allow_files = True),
    "is_executable": attr.bool(mandatory = True),
    "allow_symlink": attr.bool(mandatory = True),
    "out": attr.output(mandatory = True),
}

_copy_file = rule(
    implementation = _copy_file_impl,
    provides = [DefaultInfo],
    attrs = _ATTRS,
    toolchains = COPY_FILE_TOOLCHAINS,
)

_copy_xfile = rule(
    implementation = _copy_file_impl,
    executable = True,
    provides = [DefaultInfo],
    attrs = _ATTRS,
    toolchains = COPY_FILE_TOOLCHAINS,
)

def copy_file(name, src, out, is_executable = False, allow_symlink = False, **kwargs):
    """Copies a file or directory to another location.

    `native.genrule()` is sometimes used to copy files (often wishing to rename them). The 'copy_file' rule does this with a simpler interface than genrule.

    This rule uses a hermetic uutils/coreutils `cp` binary, no shell is required.

    If using this rule with source directories, it is recommended that you use the
    `--host_jvm_args=-DBAZEL_TRACK_SOURCE_DIRECTORIES=1` startup option so that changes
    to files within source directories are detected. See
    https://github.com/bazelbuild/bazel/commit/c64421bc35214f0414e4f4226cc953e8c55fa0d2
    for more context.

    Args:
      name: Name of the rule.
      src: A Label. The file to make a copy of.
          (Can also be the label of a rule that generates a file.)
      out: Path of the output file, relative to this package.
      is_executable: A boolean. Whether to make the output file executable. When
          True, the rule's output can be executed using `bazel run` and can be
          in the srcs of binary and test rules that require executable sources.
          WARNING: If `allow_symlink` is True, `src` must also be executable.
      allow_symlink: A boolean. Whether to allow symlinking instead of copying.
          When False, the output is always a hard copy. When True, the output
          *can* be a symlink, but there is no guarantee that a symlink is
          created (i.e., at the time of writing, we don't create symlinks on
          Windows). Set this to True if you need fast copying and your tools can
          handle symlinks (which most UNIX tools can).
      **kwargs: further keyword arguments, e.g. `visibility`
    """

    copy_file_impl = _copy_xfile if is_executable else _copy_file

    copy_file_impl(
        name = name,
        src = src,
        out = out,
        is_executable = is_executable,
        allow_symlink = allow_symlink,
        **kwargs
    )
