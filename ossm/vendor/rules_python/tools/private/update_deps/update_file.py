# Copyright 2023 The Bazel Authors. All rights reserved.
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

"""A small library to update bazel files within the repo.

This is reused in other files updating coverage deps and pip deps.
"""

import argparse
import difflib
import pathlib
import sys


def _writelines(path: pathlib.Path, out: str):
    with open(path, "w") as f:
        f.write(out)


def unified_diff(name: str, a: str, b: str) -> str:
    return "".join(
        difflib.unified_diff(
            a.splitlines(keepends=True),
            b.splitlines(keepends=True),
            fromfile=f"a/{name}",
            tofile=f"b/{name}",
        )
    ).strip()


def replace_snippet(
    current: str,
    snippet: str,
    start_marker: str,
    end_marker: str,
) -> str:
    """Update a file on disk to replace text in a file between two markers.

    Args:
        path: pathlib.Path, the path to the file to be modified.
        snippet: str, the snippet of code to insert between the markers.
        start_marker: str, the text that marks the start of the region to be replaced.
        end_markr: str, the text that marks the end of the region to be replaced.
        dry_run: bool, if set to True, then the file will not be written and instead we are going to print a diff to
            stdout.
    """
    lines = []
    skip = False
    found_match = False
    for line in current.splitlines(keepends=True):
        if line.lstrip().startswith(start_marker.lstrip()):
            found_match = True
            lines.append(line)
            lines.append(snippet.rstrip() + "\n")
            skip = True
        elif skip and line.lstrip().startswith(end_marker):
            skip = False
            lines.append(line)
            continue
        elif not skip:
            lines.append(line)

    if not found_match:
        raise RuntimeError(f"Start marker '{start_marker}' was not found")
    if skip:
        raise RuntimeError(f"End marker '{end_marker}' was not found")

    return "".join(lines)


def update_file(
    path: pathlib.Path,
    snippet: str,
    start_marker: str,
    end_marker: str,
    dry_run: bool = True,
):
    """update a file on disk to replace text in a file between two markers.

    Args:
        path: pathlib.Path, the path to the file to be modified.
        snippet: str, the snippet of code to insert between the markers.
        start_marker: str, the text that marks the start of the region to be replaced.
        end_markr: str, the text that marks the end of the region to be replaced.
        dry_run: bool, if set to True, then the file will not be written and instead we are going to print a diff to
            stdout.
    """
    current = path.read_text()
    out = replace_snippet(current, snippet, start_marker, end_marker)

    if not dry_run:
        _writelines(path, out)
        return

    relative = path.relative_to(
        pathlib.Path(__file__).resolve().parent.parent.parent.parent
    )
    name = f"{relative}"
    diff = unified_diff(name, current, out)
    if diff:
        print(f"Diff of the changes that would be made to '{name}':\n{diff}")
    else:
        print(f"'{name}' is up to date")
