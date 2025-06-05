# Copyright 2017 The Bazel Authors. All rights reserved.
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

"""This module contains providers for working with TreeArtifacts.

See https://github.com/bazelbuild/bazel-skylib/issues/300
(this feature could be upstreamed to bazel-skylib in the future)

These are also called output directories, created by `ctx.actions.declare_directory`.
"""

DirectoryFilePathInfo = provider(
    doc = "Joins a label pointing to a TreeArtifact with a path nested within that directory.",
    fields = {
        "directory": "a TreeArtifact (ctx.actions.declare_directory)",
        "path": "path relative to the directory",
    },
)

def _directory_file_path(ctx):
    if not ctx.file.directory.is_source and not ctx.file.directory.is_directory:
        fail("directory attribute must be a source directory or created with Bazel declare_directory (TreeArtifact)")
    return [DirectoryFilePathInfo(path = ctx.attr.path, directory = ctx.file.directory)]

directory_file_path = rule(
    doc = """Provide DirectoryFilePathInfo to reference some file within a directory.

        Otherwise there is no way to give a Bazel label for it.""",
    implementation = _directory_file_path,
    attrs = {
        "directory": attr.label(
            doc = "a directory",
            mandatory = True,
            allow_single_file = True,
        ),
        "path": attr.string(
            doc = "a path within that directory",
            mandatory = True,
        ),
    },
)
