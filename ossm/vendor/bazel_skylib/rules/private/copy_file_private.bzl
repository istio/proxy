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

These rules copy a file to another location using Bash (on Linux/macOS) or
cmd.exe (on Windows). '_copy_xfile' marks the resulting file executable,
'_copy_file' does not.
"""

load(":copy_common.bzl", "COPY_EXECUTION_REQUIREMENTS")

def copy_cmd(ctx, src, dst):
    # Most Windows binaries built with MSVC use a certain argument quoting
    # scheme. Bazel uses that scheme too to quote arguments. However,
    # cmd.exe uses different semantics, so Bazel's quoting is wrong here.
    # To fix that we write the command to a .bat file so no command line
    # quoting or escaping is required.
    bat = ctx.actions.declare_file(ctx.label.name + "-cmd.bat")
    ctx.actions.write(
        output = bat,
        # Do not use lib/shell.bzl's shell.quote() method, because that uses
        # Bash quoting syntax, which is different from cmd.exe's syntax.
        content = "@copy /Y \"%s\" \"%s\" >NUL" % (
            src.path.replace("/", "\\"),
            dst.path.replace("/", "\\"),
        ),
        is_executable = True,
    )
    ctx.actions.run(
        inputs = [src, bat],
        outputs = [dst],
        executable = "cmd.exe",
        arguments = ["/C", bat.path.replace("/", "\\")],
        mnemonic = "CopyFile",
        progress_message = "Copying files",
        use_default_shell_env = True,
        execution_requirements = COPY_EXECUTION_REQUIREMENTS,
    )

def copy_bash(ctx, src, dst):
    ctx.actions.run_shell(
        inputs = [src],
        outputs = [dst],
        command = "cp -f \"$1\" \"$2\"",
        arguments = [src.path, dst.path],
        mnemonic = "CopyFile",
        progress_message = "Copying files",
        use_default_shell_env = True,
        execution_requirements = COPY_EXECUTION_REQUIREMENTS,
    )

def _copy_file_impl(ctx):
    if ctx.attr.allow_symlink:
        ctx.actions.symlink(
            output = ctx.outputs.out,
            target_file = ctx.file.src,
            is_executable = ctx.attr.is_executable,
        )
    elif ctx.attr.is_windows:
        copy_cmd(ctx, ctx.file.src, ctx.outputs.out)
    else:
        copy_bash(ctx, ctx.file.src, ctx.outputs.out)

    files = depset(direct = [ctx.outputs.out])
    runfiles = ctx.runfiles(files = [ctx.outputs.out])
    if ctx.attr.is_executable:
        return [DefaultInfo(files = files, runfiles = runfiles, executable = ctx.outputs.out)]
    else:
        # Do not include the copied file into the default runfiles of the
        # target, but ensure that it is picked up by native rule's data
        # attribute despite https://github.com/bazelbuild/bazel/issues/15043.
        return [DefaultInfo(files = files, data_runfiles = runfiles)]

_ATTRS = {
    "src": attr.label(mandatory = True, allow_single_file = True),
    "out": attr.output(mandatory = True),
    "is_windows": attr.bool(mandatory = True),
    "is_executable": attr.bool(mandatory = True),
    "allow_symlink": attr.bool(mandatory = True),
}

_copy_file = rule(
    implementation = _copy_file_impl,
    provides = [DefaultInfo],
    attrs = _ATTRS,
)

_copy_xfile = rule(
    implementation = _copy_file_impl,
    executable = True,
    provides = [DefaultInfo],
    attrs = _ATTRS,
)

def copy_file(name, src, out, is_executable = False, allow_symlink = False, **kwargs):
    """Copies a file to another location.

    `native.genrule()` is sometimes used to copy files (often wishing to rename them). The 'copy_file' rule does this with a simpler interface than genrule.

    This rule uses a Bash command on Linux/macOS/non-Windows, and a cmd.exe command on Windows (no Bash is required).

    Args:
      name: Name of the rule.
      src: A Label. The file to make a copy of. (Can also be the label of a rule
          that generates a file.)
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

    copy_file_impl = _copy_file
    if is_executable:
        copy_file_impl = _copy_xfile

    copy_file_impl(
        name = name,
        src = src,
        out = out,
        is_windows = select({
            "@bazel_tools//src/conditions:host_windows": True,
            "//conditions:default": False,
        }),
        is_executable = is_executable,
        allow_symlink = allow_symlink,
        **kwargs
    )
