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

"""A test rule that compares two binary files or two directories.

Similar to `bazel-skylib`'s [`diff_test`](https://github.com/bazelbuild/bazel-skylib/blob/main/rules/diff_test.bzl)
but also supports comparing directories.

The rule uses a Bash command (diff) on Linux/macOS/non-Windows, and a cmd.exe
command (fc.exe) on Windows (no Bash is required).
"""

load("@bazel_skylib//lib:shell.bzl", "shell")
load(":directory_path.bzl", "DirectoryPathInfo")

def _runfiles_path(f):
    if f.root.path:
        return f.path[len(f.root.path) + 1:]  # generated file
    else:
        return f.path  # source file

def _diff_test_impl(ctx):
    is_windows = ctx.target_platform_has_constraint(ctx.attr._windows_constraint[platform_common.ConstraintValueInfo])

    if DirectoryPathInfo in ctx.attr.file1:
        file1 = ctx.attr.file1[DirectoryPathInfo].directory
        file1_path = "/".join([_runfiles_path(file1), ctx.attr.file1[DirectoryPathInfo].path])
    else:
        if len(ctx.files.file1) != 1:
            fail("file1 must be a single file or a target that provides a DirectoryPathInfo")
        file1 = ctx.files.file1[0]
        file1_path = _runfiles_path(file1)

    if DirectoryPathInfo in ctx.attr.file2:
        file2 = ctx.attr.file2[DirectoryPathInfo].directory
        file2_path = "/".join([_runfiles_path(file2), ctx.attr.file2[DirectoryPathInfo].path])
    else:
        if len(ctx.files.file2) != 1:
            fail("file2 must be a single file or a target that provides a DirectoryPathInfo")
        file2 = ctx.files.file2[0]
        file2_path = _runfiles_path(file2)

    if file1 == file2:
        msg = "diff_test comparing the same file %s" % file1
        fail(msg)

    if is_windows:
        test_suffix = "-test.bat"
        template = ctx.file._diff_test_tmpl_bat
    else:
        test_suffix = "-test.sh"
        template = ctx.file._diff_test_tmpl_sh

    test_bin = ctx.actions.declare_file(ctx.label.name + test_suffix)
    ctx.actions.expand_template(
        template = template,
        output = test_bin,
        substitutions = {
            "{name}": ctx.attr.name,
            "{fail_msg}": ctx.attr.failure_message,
            "{file1}": file1_path,
            "{file2}": file2_path,
            "{build_file_path}": ctx.build_file_path,
            "{diff_args}": " ".join([
                shell.quote(arg)
                for arg in ctx.attr.diff_args
            ]),
        },
        is_executable = True,
    )

    return DefaultInfo(
        executable = test_bin,
        files = depset(direct = [test_bin]),
        runfiles = ctx.runfiles(files = [test_bin, file1, file2]),
    )

_diff_test = rule(
    attrs = {
        "failure_message": attr.string(),
        "file1": attr.label(
            allow_files = True,
            mandatory = True,
        ),
        "file2": attr.label(
            allow_files = True,
            mandatory = True,
        ),
        "diff_args": attr.string_list(),
        "_windows_constraint": attr.label(default = "@platforms//os:windows"),
        "_diff_test_tmpl_sh": attr.label(
            default = ":diff_test_tmpl.sh",
            allow_single_file = True,
        ),
        "_diff_test_tmpl_bat": attr.label(
            default = ":diff_test_tmpl.bat",
            allow_single_file = True,
        ),
    },
    test = True,
    implementation = _diff_test_impl,
)

def diff_test(name, file1, file2, diff_args = [], size = "small", **kwargs):
    """A test that compares two files.

    The test succeeds if the files' contents match.

    Args:
      name: The name of the test rule.
      file1: Label of the file to compare to <code>file2</code>.
      file2: Label of the file to compare to <code>file1</code>.
      diff_args: Arguments to pass to the `diff` command. (Ignored on Windows)
      size: standard attribute for tests
      **kwargs: The <a href="https://docs.bazel.build/versions/main/be/common-definitions.html#common-attributes-tests">common attributes for tests</a>.
    """
    _diff_test(
        name = name,
        file1 = file1,
        file2 = file2,
        size = size,
        diff_args = diff_args,
        **kwargs
    )
