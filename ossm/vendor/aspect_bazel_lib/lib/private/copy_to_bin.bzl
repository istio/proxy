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

"""Implementation of copy_to_bin macro and underlying rules."""

load("@bazel_skylib//lib:paths.bzl", "paths")
load(":copy_file.bzl", "COPY_FILE_TOOLCHAINS", "copy_file_action")

COPY_FILE_TO_BIN_TOOLCHAINS = COPY_FILE_TOOLCHAINS

def copy_file_to_bin_action(ctx, file):
    """Factory function that creates an action to copy a file to the output tree.

    File are copied to the same workspace-relative path. The resulting files is
    returned.

    If the file passed in is already in the output tree is then it is returned
    without a copy action.

    To use `copy_file_to_bin_action` in your own rules, you need to include the toolchains it uses
    in your rule definition. For example:

    ```starlark
    load("@aspect_bazel_lib//lib:copy_to_bin.bzl", "COPY_FILE_TO_BIN_TOOLCHAINS")

    my_rule = rule(
        ...,
        toolchains = COPY_FILE_TO_BIN_TOOLCHAINS,
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
        file: The file to copy.

    Returns:
        A File in the output tree.
    """

    if not file.is_source:
        return file
    if ctx.label.workspace_name != file.owner.workspace_name:
        fail(_file_in_external_repo_error_msg(file))
    if ctx.label.package != file.owner.package:
        fail(_file_in_different_package_error_msg(file, ctx.label))

    if file.path.startswith("bazel-"):
        first = file.path.split("/")[0]
        suffix = first[len("bazel-"):]
        if suffix in ["testlogs", "bin", "out"]:
            # buildifier: disable=print
            print("""
WARNING: sources are being copied from {bin}. This is probably not what you want.

Add these lines to .bazelignore:
bazel-out
bazel-bin
bazel-testlogs

and/or correct the `glob` patterns that are including these files in the sources.
""".format(bin = first))

    dst = ctx.actions.declare_file(file.basename, sibling = file)
    copy_file_action(ctx, file, dst)
    return dst

def _file_in_external_repo_error_msg(file):
    return """
Cannot use copy_to_bin to copy {file_basename} from the external repository @{repository}.
Files can only be copied from the source tree to their short path equivalent in the output tree.
""".format(
        file_basename = file.basename,
        repository = file.owner.workspace_name,
    )

def _file_in_different_package_error_msg(file, curr_package_label):
    return """
Expected to find file {file_basename} in {package}, but instead it is in {file_package}.

To use copy_to_bin, either move {file_basename} to {package}, or move the copy_to_bin
target to {file_package} using:

    buildozer 'new copy_to_bin {target_name}' {file_package}:__pkg__
    buildozer 'add srcs {file_basename}' {file_package}:{target_name}
    buildozer 'new_load @aspect_bazel_lib//lib:copy_to_bin.bzl copy_to_bin' {file_package}:__pkg__
    buildozer 'add visibility {package}:__subpackages__' {file_package}:{target_name}

    """.format(
        file_basename = file.basename,
        file_package = "%s//%s" % (file.owner.workspace_name, file.owner.package),
        target_name = paths.replace_extension(file.basename, ""),
        package = "%s//%s" % (curr_package_label.workspace_name, curr_package_label.package),
    )

def copy_files_to_bin_actions(ctx, files):
    """Factory function that creates actions to copy files to the output tree.

    Files are copied to the same workspace-relative path. The resulting list of
    files is returned.

    If a file passed in is already in the output tree is then it is added
    directly to the result without a copy action.

    Args:
        ctx: The rule context.
        files: List of File objects.

    Returns:
        List of File objects in the output tree.
    """

    return [copy_file_to_bin_action(ctx, file) for file in files]

def _copy_to_bin_impl(ctx):
    files = copy_files_to_bin_actions(ctx, ctx.files.srcs)
    return DefaultInfo(
        files = depset(files),
        runfiles = ctx.runfiles(files = files),
    )

_copy_to_bin = rule(
    implementation = _copy_to_bin_impl,
    provides = [DefaultInfo],
    attrs = {
        "srcs": attr.label_list(mandatory = True, allow_files = True),
    },
    toolchains = COPY_FILE_TO_BIN_TOOLCHAINS,
)

def copy_to_bin(name, srcs, **kwargs):
    """Copies a source file to output tree at the same workspace-relative path.

    e.g. `<execroot>/path/to/file -> <execroot>/bazel-out/<platform>/bin/path/to/file`

    If a file passed in is already in the output tree is then it is added directly to the
    DefaultInfo provided by the rule without a copy.

    This is useful to populate the output folder with all files needed at runtime, even
    those which aren't outputs of a Bazel rule.

    This way you can run a binary in the output folder (execroot or runfiles_root)
    without that program needing to rely on a runfiles helper library or be aware that
    files are divided between the source tree and the output tree.

    Args:
        name: Name of the rule.
        srcs: A list of labels. File(s) to copy.
        **kwargs: further keyword arguments, e.g. `visibility`
    """
    _copy_to_bin(
        name = name,
        srcs = srcs,
        **kwargs
    )
