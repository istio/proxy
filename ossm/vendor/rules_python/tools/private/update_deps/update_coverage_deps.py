#!/usr/bin/python3 -B
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

"""A small script to update bazel files within the repo.

We are not running this with 'bazel run' to keep the dependencies minimal
"""

# NOTE @aignas 2023-01-09: We should only depend on core Python 3 packages.
import argparse
import difflib
import json
import os
import pathlib
import sys
import textwrap
from collections import defaultdict
from dataclasses import dataclass
from typing import Any
from urllib import request

from tools.private.update_deps.args import path_from_runfiles
from tools.private.update_deps.update_file import update_file

# This should be kept in sync with //python:versions.bzl
_supported_platforms = {
    # Windows is unsupported right now
    # "win_amd64": "x86_64-pc-windows-msvc",
    "manylinux2014_x86_64": "x86_64-unknown-linux-gnu",
    "manylinux2014_aarch64": "aarch64-unknown-linux-gnu",
    "macosx_11_0_arm64": "aarch64-apple-darwin",
    "macosx_10_9_x86_64": "x86_64-apple-darwin",
    ("t", "manylinux2014_x86_64"): "x86_64-unknown-linux-gnu-freethreaded",
    ("t", "manylinux2014_aarch64"): "aarch64-unknown-linux-gnu-freethreaded",
    ("t", "macosx_11_0_arm64"): "aarch64-apple-darwin-freethreaded",
    ("t", "macosx_10_9_x86_64"): "x86_64-apple-darwin-freethreaded",
}


@dataclass
class Dep:
    name: str
    platform: str
    python: str
    url: str
    sha256: str

    @property
    def repo_name(self):
        return f"pypi__{self.name}_{self.python}_{self.platform}"

    def __repr__(self):
        return "\n".join(
            [
                "(",
                f'    "{self.url}",',
                f'    "{self.sha256}",',
                ")",
            ]
        )


@dataclass
class Deps:
    deps: list[Dep]

    def __repr__(self):
        deps = defaultdict(dict)
        for d in self.deps:
            deps[d.python][d.platform] = d

        parts = []
        for python, contents in deps.items():
            inner = textwrap.indent(
                "\n".join([f'"{platform}": {d},' for platform, d in contents.items()]),
                prefix="    ",
            )
            parts.append('"{}": {{\n{}\n}},'.format(python, inner))
        return "{{\n{}\n}}".format(textwrap.indent("\n".join(parts), prefix="    "))


def _get_platforms(filename: str, python_version: str):
    name, _, tail = filename.partition("-")
    version, _, tail = tail.partition("-")
    got_python_version, _, tail = tail.partition("-")
    if python_version != got_python_version:
        return []
    abi, _, tail = tail.partition("-")

    platforms, _, tail = tail.rpartition(".")
    platforms = platforms.split(".")

    return [("t", p) for p in platforms] if abi.endswith("t") else platforms


def _map(
    name: str,
    filename: str,
    python_version: str,
    url: str,
    digests: list,
    platform: str,
    **kwargs: Any,
):
    if platform not in _supported_platforms:
        return None

    return Dep(
        name=name,
        platform=_supported_platforms[platform],
        python=python_version,
        url=url,
        sha256=digests["sha256"],
    )


def _parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument(
        "--name",
        default="coverage",
        type=str,
        help="The name of the package",
    )
    parser.add_argument(
        "version",
        type=str,
        help="The version of the package to download",
    )
    parser.add_argument(
        "--py",
        nargs="+",
        type=str,
        default=["cp38", "cp39", "cp310", "cp311", "cp312", "cp313"],
        help="Supported python versions",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Whether to write to files",
    )
    parser.add_argument(
        "--update-file",
        type=path_from_runfiles,
        default=os.environ.get("UPDATE_FILE"),
        help="The path for the file to be updated, defaults to the value taken from UPDATE_FILE",
    )
    return parser.parse_args()


def main():
    args = _parse_args()

    api_url = f"https://pypi.org/pypi/{args.name}/{args.version}/json"
    req = request.Request(api_url)
    with request.urlopen(req) as response:
        data = json.loads(response.read().decode("utf-8"))

    urls = []
    for u in data["urls"]:
        if u["yanked"]:
            continue

        if not u["filename"].endswith(".whl"):
            continue

        if u["python_version"] not in args.py:
            continue

        if f'_{u["python_version"]}m_' in u["filename"]:
            continue

        platforms = _get_platforms(
            u["filename"],
            u["python_version"],
        )

        result = [_map(name=args.name, platform=p, **u) for p in platforms]
        urls.extend(filter(None, result))

    urls.sort(key=lambda x: f"{x.python}_{x.platform}")

    # Update the coverage_deps, which are used to register deps
    update_file(
        path=args.update_file,
        snippet=f"_coverage_deps = {repr(Deps(urls))}\n",
        start_marker="# START: maintained by 'bazel run //tools/private/update_deps:update_coverage_deps <version>'",
        end_marker="# END: maintained by 'bazel run //tools/private/update_deps:update_coverage_deps <version>'",
        dry_run=args.dry_run,
    )

    return


if __name__ == "__main__":
    main()
