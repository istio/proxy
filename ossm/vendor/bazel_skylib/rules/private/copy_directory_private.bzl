# Copyright 2022 The Bazel Authors. All rights reserved.
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

"""Implementation of copy_directory macro and underlying rules.

This rule copies a directory to another location using Bash (on Linux/macOS) or
cmd.exe (on Windows).
"""

load(":copy_common.bzl", "COPY_EXECUTION_REQUIREMENTS")

def _copy_cmd(ctx, src, dst):
    # Most Windows binaries built with MSVC use a certain argument quoting
    # scheme. Bazel uses that scheme too to quote arguments. However,
    # cmd.exe uses different semantics, so Bazel's quoting is wrong here.
    # To fix that we write the command to a .bat file so no command line
    # quoting or escaping is required.
    # Put a hash of the file name into the name of the generated batch file to
    # make it unique within the package, so that users can define multiple copy_file's.
    bat = ctx.actions.declare_file("%s-%s-cmd.bat" % (ctx.label.name, hash(src.path)))

    # Flags are documented at
    # https://docs.microsoft.com/en-us/windows-server/administration/windows-commands/robocopy
    # NB: robocopy return non-zero exit codes on success so we must exit 0 after calling it
    cmd_tmpl = """\
if not exist \"{src}\\\" (
  echo Error: \"{src}\" is not a directory
  @exit 1
)
@robocopy \"{src}\" \"{dst}\" /E /MIR >NUL & @exit 0
"""
    mnemonic = "CopyDirectory"
    progress_message = "Copying directory %{input}"

    ctx.actions.write(
        output = bat,
        # Do not use lib/shell.bzl's shell.quote() method, because that uses
        # Bash quoting syntax, which is different from cmd.exe's syntax.
        content = cmd_tmpl.format(
            src = src.path.replace("/", "\\"),
            dst = dst.path.replace("/", "\\"),
        ),
        is_executable = True,
    )
    ctx.actions.run(
        inputs = [src, bat],
        outputs = [dst],
        executable = "cmd.exe",
        arguments = ["/C", bat.path.replace("/", "\\")],
        mnemonic = mnemonic,
        progress_message = progress_message,
        use_default_shell_env = True,
        execution_requirements = COPY_EXECUTION_REQUIREMENTS,
    )

def _copy_bash(ctx, src, dst):
    cmd = """\
if [ ! -d \"$1\" ]; then
    echo \"Error: $1 is not a directory\"
    exit 1
fi

rm -rf \"$2\" && cp -fR \"$1/\" \"$2\"
"""
    mnemonic = "CopyDirectory"
    progress_message = "Copying directory %s" % src.path

    ctx.actions.run_shell(
        inputs = [src],
        outputs = [dst],
        command = cmd,
        arguments = [src.path, dst.path],
        mnemonic = mnemonic,
        progress_message = progress_message,
        use_default_shell_env = True,
        execution_requirements = COPY_EXECUTION_REQUIREMENTS,
    )

def copy_directory_action(ctx, src, dst, is_windows = False):
    """Helper function that creates an action to copy a directory from src to dst.

    This helper is used by copy_directory. It is exposed as a public API so it can be used within
    other rule implementations.

    Args:
        ctx: The rule context.
        src: The directory to make a copy of. Can be a source directory or TreeArtifact.
        dst: The directory to copy to. Must be a TreeArtifact.
        is_windows: If true, an cmd.exe action is created so there is no bash dependency.
    """
    if dst.is_source or not dst.is_directory:
        fail("dst must be a TreeArtifact")
    if is_windows:
        _copy_cmd(ctx, src, dst)
    else:
        _copy_bash(ctx, src, dst)

def _copy_directory_impl(ctx):
    dst = ctx.actions.declare_directory(ctx.attr.out)
    copy_directory_action(ctx, ctx.file.src, dst, ctx.attr.is_windows)

    files = depset(direct = [dst])
    runfiles = ctx.runfiles(files = [dst])

    return [DefaultInfo(files = files, runfiles = runfiles)]

_copy_directory = rule(
    implementation = _copy_directory_impl,
    provides = [DefaultInfo],
    attrs = {
        "src": attr.label(mandatory = True, allow_single_file = True),
        "is_windows": attr.bool(mandatory = True),
        # Cannot declare out as an output here, because there's no API for declaring
        # TreeArtifact outputs.
        "out": attr.string(mandatory = True),
    },
)

def copy_directory(name, src, out, **kwargs):
    """Copies a directory to another location.

    This rule uses a Bash command on Linux/macOS/non-Windows, and a cmd.exe command on Windows (no Bash is required).

    If using this rule with source directories, it is recommended that you use the
    `--host_jvm_args=-DBAZEL_TRACK_SOURCE_DIRECTORIES=1` startup option so that changes
    to files within source directories are detected. See
    https://github.com/bazelbuild/bazel/commit/c64421bc35214f0414e4f4226cc953e8c55fa0d2
    for more context.

    Args:
      name: Name of the rule.
      src: The directory to make a copy of. Can be a source directory or TreeArtifact.
      out: Path of the output directory, relative to this package.
      **kwargs: further keyword arguments, e.g. `visibility`
    """
    _copy_directory(
        name = name,
        src = src,
        is_windows = select({
            "@bazel_tools//src/conditions:host_windows": True,
            "//conditions:default": False,
        }),
        out = out,
        **kwargs
    )
