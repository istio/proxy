# Copyright 2024 The Bazel Authors. All rights reserved.
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

"""Skylib module containing providers for directories."""

load("//rules/directory/private:glob.bzl", "glob")
load("//rules/directory/private:paths.bzl", "DIRECTORY", "FILE", "get_path")

def _init_directory_info(**kwargs):
    self = struct(**kwargs)
    kwargs.update(
        get_path = lambda path: get_path(self, path, require_type = None),
        get_file = lambda path: get_path(self, path, require_type = FILE),
        get_subdirectory = lambda path: get_path(self, path, require_type = DIRECTORY),
        glob = lambda include, exclude = [], allow_empty = False: glob(self, include, exclude, allow_empty),
    )
    return kwargs

# TODO: Once bazel 5 no longer needs to be supported, remove this function, and add
# init = _init_directory_info to the provider below
# buildifier: disable=function-docstring
def create_directory_info(**kwargs):
    return DirectoryInfo(**_init_directory_info(**kwargs))

DirectoryInfo = provider(
    doc = "Information about a directory",
    # @unsorted-dict-items
    fields = {
        "entries": "(Dict[str, Either[File, DirectoryInfo]]) The entries contained directly within. Ordered by filename",
        "transitive_files": "(depset[File]) All files transitively contained within this directory.",
        "path": "(string) Path to all files contained within this directory.",
        "human_readable": "(string) A human readable identifier for a directory. Useful for providing error messages to a user.",
        "get_path": "(Function(str) -> DirectoryInfo|File) A function to return the entry corresponding to the joined path.",
        "get_file": "(Function(str) -> File) A function to return the entry corresponding to the joined path.",
        "get_subdirectory": "(Function(str) -> DirectoryInfo) A function to return the entry corresponding to the joined path.",
        "glob": "(Function(include, exclude, allow_empty=False)) A function that works the same as native.glob.",
    },
)
