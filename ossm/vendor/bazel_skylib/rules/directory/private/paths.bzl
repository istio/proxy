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

"""Skylib module containing path operations on directories."""

load("//lib:paths.bzl", "paths")

_NOT_FOUND = """{directory} does not contain an entry named {name}.
Instead, it contains the following entries:
{children}

"""
_WRONG_TYPE = "Expected {dir}/{name} to have type {want}, but got {got}"

# These correspond to an "enum".
FILE = "file"
DIRECTORY = "directory"

def _check_path_relative(path):
    if paths.is_absolute(path):
        fail("Path must be relative. Got {path}".format(path = path))

def _get_direct_child(directory, name, require_type = None):
    """Gets the direct child of a directory.

    Args:
        directory: (DirectoryInfo) The directory to look within.
        name: (string) The name of the directory/file to look for.
        require_type: (Optional[DIRECTORY|FILE]) If provided, must return
          either a the corresponding type.

    Returns:
        (File|DirectoryInfo) The content contained within.
    """
    entry = directory.entries.get(name, None)
    if entry == None:
        fail(_NOT_FOUND.format(
            directory = directory.human_readable,
            name = repr(name),
            children = "\n".join(directory.entries.keys()),
        ))
    if require_type == DIRECTORY and type(entry) == "File":
        fail(_WRONG_TYPE.format(
            dir = directory.human_readable,
            name = name,
            want = "Directory",
            got = "File",
        ))

    if require_type == FILE and type(entry) != "File":
        fail(_WRONG_TYPE.format(
            dir = directory.human_readable,
            name = name,
            want = "File",
            got = "Directory",
        ))
    return entry

def get_path(directory, path, require_type = None):
    """Gets a subdirectory or file contained within a directory.

    Example: `get_path(directory, "a/b", require_type=FILE)`
      -> the file  corresponding to `directory.path + "/a/b"`

    Args:
        directory: (DirectoryInfo) The directory to look within.
        path: (string) The path of the directory to look for within it.
        require_type: (Optional[DIRECTORY|FILE]) If provided, must return
          either a the corresponding type.

    Returns:
        (File|DirectoryInfo) The directory contained within.
    """
    _check_path_relative(path)

    chunks = path.split("/")
    for dirname in chunks[:-1]:
        directory = _get_direct_child(directory, dirname, require_type = DIRECTORY)
    return _get_direct_child(
        directory,
        chunks[-1],
        require_type = require_type,
    )
