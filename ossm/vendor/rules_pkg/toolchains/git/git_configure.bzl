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
"""Repository rule to autoconfigure a toolchain using the system git."""

def _write_build(rctx, path, workspace_dir):
    if not path:
        path = ""
    rctx.template(
        "BUILD",
        Label("//toolchains/git:BUILD.tpl"),
        substitutions = {
            "{GENERATOR}": "@rules_pkg//toolchains/git/git_configure.bzl%find_system_git",
            "{GIT_PATH}": str(path),
            "{GIT_ROOT}": workspace_dir,
        },
        executable = False,
    )

def _find_system_git_impl(rctx):
    git_path = rctx.which("git")
    if rctx.attr.verbose:
        if git_path:
            print("Found git at '%s'" % git_path)  # buildifier: disable=print
        else:
            print("No system git found.")  # buildifier: disable=print

    # In a conventional setup the directory of the WORKSPACE file is under the git client.
    # So we use the absolute path to WORKSPACE dir as a surrogate for git-ness.
    ws_dir = str(rctx.path(rctx.attr.workspace_file).dirname.realpath)
    if rctx.attr.verbose:
        print("Found WORKSPACE in", ws_dir)  # buildifier: disable=print
    _write_build(rctx = rctx, path = git_path, workspace_dir = ws_dir)

_find_system_git = repository_rule(
    implementation = _find_system_git_impl,
    doc = """Create a repository that defines an git toolchain based on the system git.""",
    local = True,
    attrs = {
        "workspace_file": attr.label(
            doc = "Reference to calling repository WORKSPACE file.",
            allow_single_file = True,
            mandatory = True,
        ),
        "verbose": attr.bool(
            doc = "If true, print status messages.",
        ),
    },
)

# buildifier: disable=function-docstring-args
def experimental_find_system_git(name, workspace_file = None, verbose = False):
    """Create a toolchain that lets you run git.

    WARNING: This is experimental. The API and behavior are subject to change
    at any time.

    This presumes that your Bazel WORKSPACE file is located under your git
    client. That is often true, but might not be in a multi-repo where you
    might weave together a Bazel workspace from several git repos that are
    all rooted under the WORKSPACE file.
    """
    if not workspace_file:
        workspace_file = Label("//:WORKSPACE")
    _find_system_git(name = name, workspace_file = workspace_file, verbose = verbose)
    native.register_toolchains(
        "@%s//:git_auto_toolchain" % name,
        "@rules_pkg//toolchains/git:git_missing_toolchain",
    )

# buildifier: disable=function-docstring-args
def experimental_find_system_git_bzlmod(name, workspace_file = None, verbose = False):
    """Create a toolchain that lets you run git.

    WARNING: This is experimental. The API and behavior are subject to change
    at any time.

    This presumes that your Bazel WORKSPACE file is located under your git
    client. That is often true, but might not be in a multi-repo where you
    might weave together a Bazel workspace from several git repos that are
    all rooted under the WORKSPACE file.
    """
    if not workspace_file:
        workspace_file = Label("//:MODULE.bazel")
    _find_system_git(name = name, workspace_file = workspace_file, verbose = verbose)
