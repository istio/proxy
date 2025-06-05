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

"""Implementation of write_file macro and underlying rules.

These rules write a UTF-8 encoded text file, using Bazel's FileWriteAction.
'_write_xfile' marks the resulting file executable, '_write_file' does not.
"""

def _common_impl(ctx, is_windows, is_executable):
    if ctx.attr.newline == "auto":
        newline = "\r\n" if is_windows else "\n"
    elif ctx.attr.newline == "windows":
        newline = "\r\n"
    else:
        newline = "\n"

    # ctx.actions.write creates a FileWriteAction which uses UTF-8 encoding.
    ctx.actions.write(
        output = ctx.outputs.out,
        content = newline.join(ctx.attr.content) if ctx.attr.content else "",
        is_executable = is_executable,
    )
    files = depset(direct = [ctx.outputs.out])
    runfiles = ctx.runfiles(files = [ctx.outputs.out])
    if is_executable:
        return [DefaultInfo(files = files, runfiles = runfiles, executable = ctx.outputs.out)]
    else:
        # Do not include the copied file into the default runfiles of the
        # target, but ensure that it is picked up by native rule's data
        # attribute despite https://github.com/bazelbuild/bazel/issues/15043.
        return [DefaultInfo(files = files, data_runfiles = runfiles)]

def _impl(ctx):
    return _common_impl(ctx, ctx.attr.is_windows, False)

def _ximpl(ctx):
    return _common_impl(ctx, ctx.attr.is_windows, True)

_ATTRS = {
    "out": attr.output(mandatory = True),
    "content": attr.string_list(mandatory = False, allow_empty = True),
    "newline": attr.string(values = ["unix", "windows", "auto"], default = "auto"),
    "is_windows": attr.bool(mandatory = True),
}

_write_file = rule(
    implementation = _impl,
    provides = [DefaultInfo],
    attrs = _ATTRS,
)

_write_xfile = rule(
    implementation = _ximpl,
    executable = True,
    provides = [DefaultInfo],
    attrs = _ATTRS,
)

def write_file(
        name,
        out,
        content = [],
        is_executable = False,
        newline = "auto",
        **kwargs):
    """Creates a UTF-8 encoded text file.

    Args:
      name: Name of the rule.
      out: Path of the output file, relative to this package.
      content: A list of strings. Lines of text, the contents of the file.
          Newlines are added automatically after every line except the last one.
      is_executable: A boolean. Whether to make the output file executable.
          When True, the rule's output can be executed using `bazel run` and can
          be in the srcs of binary and test rules that require executable
          sources.
      newline: one of ["auto", "unix", "windows"]: line endings to use. "auto"
          for platform-determined, "unix" for LF, and "windows" for CRLF.
      **kwargs: further keyword arguments, e.g. `visibility`
    """
    if is_executable:
        _write_xfile(
            name = name,
            content = content,
            out = out,
            newline = newline or "auto",
            is_windows = select({
                "@bazel_tools//src/conditions:host_windows": True,
                "//conditions:default": False,
            }),
            **kwargs
        )
    else:
        _write_file(
            name = name,
            content = content,
            out = out,
            newline = newline or "auto",
            is_windows = select({
                "@bazel_tools//src/conditions:host_windows": True,
                "//conditions:default": False,
            }),
            **kwargs
        )
