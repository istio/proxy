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

"""Skylib module containing glob operations on directories."""

_NO_GLOB_MATCHES = "{glob} failed to match any files in {dir}"

def transitive_entries(directory):
    """Returns the files and directories contained within a directory transitively.

    Args:
        directory: (DirectoryInfo) The directory to look at

    Returns:
        List[Either[DirectoryInfo, File]] The entries contained within.
    """
    entries = [directory]
    stack = [directory]
    for _ in range(99999999):
        if not stack:
            return entries
        d = stack.pop()
        for entry in d.entries.values():
            entries.append(entry)
            if type(entry) != "File":
                stack.append(entry)

    fail("Should never get to here")

def directory_glob_chunk(directory, chunk):
    """Given a directory and a chunk of a glob, returns possible candidates.

    Args:
        directory: (DirectoryInfo) The directory to look relative from.
        chunk: (string) A chunk of a glob to look at.

    Returns:
        List[Either[DirectoryInfo, File]]] The candidate next entries for the chunk.
    """
    if chunk == "*":
        return directory.entries.values()
    elif chunk == "**":
        return transitive_entries(directory)
    elif "*" not in chunk:
        if chunk in directory.entries:
            return [directory.entries[chunk]]
        else:
            return []
    elif chunk.count("*") > 2:
        fail("glob chunks with more than two asterixes are unsupported. Got", chunk)

    if chunk.count("*") == 2:
        left, middle, right = chunk.split("*")
    else:
        middle = ""
        left, right = chunk.split("*")
    entries = []
    for name, entry in directory.entries.items():
        if name.startswith(left) and name.endswith(right) and len(left) + len(right) <= len(name) and middle in name[len(left):len(name) - len(right)]:
            entries.append(entry)
    return entries

def directory_single_glob(directory, glob):
    """Calculates all files that are matched by a glob on a directory.

    Args:
        directory: (DirectoryInfo) The directory to look relative from.
        glob: (string) A glob to match.

    Returns:
        List[File] A list of files that match.
    """

    # Treat a glob as a nondeterministic finite state automata. We can be in
    # multiple places at the one time.
    candidate_dirs = [directory]
    candidate_files = []
    for chunk in glob.split("/"):
        next_candidate_dirs = {}
        candidate_files = {}
        for candidate in candidate_dirs:
            for e in directory_glob_chunk(candidate, chunk):
                if type(e) == "File":
                    candidate_files[e] = None
                else:
                    next_candidate_dirs[e.human_readable] = e
        candidate_dirs = next_candidate_dirs.values()

    return list(candidate_files)

def glob(directory, include, exclude = [], allow_empty = False):
    """native.glob, but for DirectoryInfo.

    Args:
        directory: (DirectoryInfo) The directory to look relative from.
        include: (List[string]) A list of globs to match.
        exclude: (List[string]) A list of globs to exclude.
        allow_empty: (bool) Whether to allow a glob to not match any files.

    Returns:
        depset[File] A set of files that match.
    """
    include_files = []
    for g in include:
        matches = directory_single_glob(directory, g)
        if not matches and not allow_empty:
            fail(_NO_GLOB_MATCHES.format(
                glob = repr(g),
                dir = directory.human_readable,
            ))
        include_files.extend(matches)

    if not exclude:
        return depset(include_files)

    include_files = {k: None for k in include_files}
    for g in exclude:
        matches = directory_single_glob(directory, g)
        if not matches and not allow_empty:
            fail(_NO_GLOB_MATCHES.format(
                glob = repr(g),
                dir = directory.human_readable,
            ))
        for f in matches:
            include_files.pop(f, None)
    return depset(include_files.keys())
