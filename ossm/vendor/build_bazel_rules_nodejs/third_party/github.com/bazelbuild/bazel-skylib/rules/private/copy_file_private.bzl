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

# LOCAL MODIFICATIONS:
# this has two PRs patched in:
# https://github.com/bazelbuild/bazel-skylib/pull/323
# https://github.com/bazelbuild/bazel-skylib/pull/324

"""Implementation of copy_file macro and underlying rules.

These rules copy a file or directory to another location using Bash (on Linux/macOS) or
cmd.exe (on Windows). `_copy_xfile` marks the resulting file executable,
`_copy_file` does not.
"""

load(":rules/private/copy_common_private.bzl", _COPY_EXECUTION_REQUIREMENTS = "COPY_EXECUTION_REQUIREMENTS")

def _hash_file(file):
    return str(hash(file.path))

# buildifier: disable=function-docstring
def copy_cmd(ctx, src, dst):
    # Most Windows binaries built with MSVC use a certain argument quoting
    # scheme. Bazel uses that scheme too to quote arguments. However,
    # cmd.exe uses different semantics, so Bazel's quoting is wrong here.
    # To fix that we write the command to a .bat file so no command line
    # quoting or escaping is required.
    # Put a hash of the file name into the name of the generated batch file to
    # make it unique within the package, so that users can define multiple copy_file's.
    bat = ctx.actions.declare_file("%s-%s-cmd.bat" % (ctx.label.name, _hash_file(src)))

    # Flags are documented at
    # https://docs.microsoft.com/en-us/windows-server/administration/windows-commands/copy
    # https://docs.microsoft.com/en-us/windows-server/administration/windows-commands/robocopy
    # NB: robocopy return non-zero exit codes on success so we must exit 0 after calling it
    if dst.is_directory:
        cmd_tmpl = "@robocopy \"{src}\" \"{dst}\" /E >NUL & @exit 0"
        mnemonic = "CopyDirectory"
        progress_message = "Copying directory %s" % src.path
    else:
        cmd_tmpl = "@copy /Y \"{src}\" \"{dst}\" >NUL"
        mnemonic = "CopyFile"
        progress_message = "Copying file %s" % src.path

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
        inputs = [src],
        tools = [bat],
        outputs = [dst],
        executable = "cmd.exe",
        arguments = ["/C", bat.path.replace("/", "\\")],
        mnemonic = mnemonic,
        progress_message = progress_message,
        use_default_shell_env = True,
        execution_requirements = _COPY_EXECUTION_REQUIREMENTS,
    )

# buildifier: disable=function-docstring
def copy_bash(ctx, src, dst):
    if dst.is_directory:
        cmd_tmpl = "rm -rf \"$2\" && cp -fR \"$1/\" \"$2\""
        mnemonic = "CopyDirectory"
        progress_message = "Copying directory %s" % src.path
    else:
        cmd_tmpl = "cp -f \"$1\" \"$2\""
        mnemonic = "CopyFile"
        progress_message = "Copying file %s" % src.path

    ctx.actions.run_shell(
        tools = [src],
        outputs = [dst],
        command = cmd_tmpl,
        arguments = [src.path, dst.path],
        mnemonic = mnemonic,
        progress_message = progress_message,
        use_default_shell_env = True,
        execution_requirements = _COPY_EXECUTION_REQUIREMENTS,
    )

def _copy_file_impl(ctx):
    # When creating a directory, declare that to Bazel so downstream rules
    # see it as a TreeArtifact and handle correctly, e.g. for remote execution
    if getattr(ctx.attr, "is_directory", False):
        output = ctx.actions.declare_directory(ctx.attr.out)
    else:
        output = ctx.outputs.out
    if ctx.attr.allow_symlink:
        if output.is_directory:
            fail("Cannot use both is_directory and allow_symlink")
        ctx.actions.symlink(
            output = output,
            target_file = ctx.file.src,
            is_executable = ctx.attr.is_executable,
        )
    elif ctx.attr.is_windows:
        copy_cmd(ctx, ctx.file.src, output)
    else:
        copy_bash(ctx, ctx.file.src, output)

    files = depset(direct = [output])
    runfiles = ctx.runfiles(files = [output])
    if ctx.attr.is_executable:
        return [DefaultInfo(files = files, runfiles = runfiles, executable = output)]
    else:
        return [DefaultInfo(files = files, runfiles = runfiles)]

_ATTRS = {
    "src": attr.label(mandatory = True, allow_single_file = True),
    "is_windows": attr.bool(mandatory = True),
    "is_executable": attr.bool(mandatory = True),
    "allow_symlink": attr.bool(mandatory = True),
}

_copy_directory = rule(
    implementation = _copy_file_impl,
    provides = [DefaultInfo],
    attrs = dict(_ATTRS, **{
        "is_directory": attr.bool(default = True),
        # Cannot declare out as an output here, because there's no API for declaring
        # TreeArtifact outputs.
        "out": attr.string(mandatory = True),
    }),
)

_copy_file = rule(
    implementation = _copy_file_impl,
    provides = [DefaultInfo],
    attrs = dict(_ATTRS, **{
        "out": attr.output(mandatory = True),
    }),
)

_copy_xfile = rule(
    implementation = _copy_file_impl,
    executable = True,
    provides = [DefaultInfo],
    attrs = dict(_ATTRS, **{
        "out": attr.output(mandatory = True),
    }),
)

def copy_file(name, src, out, is_directory = False, is_executable = False, allow_symlink = False, **kwargs):
    """Copies a file or directory to another location.

    `native.genrule()` is sometimes used to copy files (often wishing to rename them). The 'copy_file' rule does this with a simpler interface than genrule.

    This rule uses a Bash command on Linux/macOS/non-Windows, and a cmd.exe command on Windows (no Bash is required).

    If using this rule with source directories, it is recommended that you use the
    `--host_jvm_args=-DBAZEL_TRACK_SOURCE_DIRECTORIES=1` startup option so that changes
    to files within source directories are detected. See
    https://github.com/bazelbuild/bazel/commit/c64421bc35214f0414e4f4226cc953e8c55fa0d2
    for more context.

    Args:
      name: Name of the rule.
      src: A Label. The file or directory to make a copy of.
          (Can also be the label of a rule that generates a file or directory.)
      out: Path of the output file, relative to this package.
      is_directory: treat the source file as a directory
          Workaround for https://github.com/bazelbuild/bazel/issues/12954
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
    elif is_directory:
        copy_file_impl = _copy_directory

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
