#!/usr/bin/env python3
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

"""A script to manage internal pip dependencies."""

from __future__ import annotations

import argparse
import json
import os
import pathlib
import re
import sys
import tempfile
import textwrap
from dataclasses import dataclass

from pip._internal.cli.main import main as pip_main

from tools.private.update_deps.args import path_from_runfiles
from tools.private.update_deps.update_file import update_file


@dataclass
class Dep:
    name: str
    url: str
    sha256: str


def _dep_snippet(deps: list[Dep]) -> str:
    lines = []
    for dep in deps:
        lines.extend(
            [
                "(\n",
                f'    "{dep.name}",\n',
                f'    "{dep.url}",\n',
                f'    "{dep.sha256}",\n',
                "),\n",
            ]
        )

    return textwrap.indent("".join(lines), " " * 4)


def _module_snippet(deps: list[Dep]) -> str:
    lines = []
    for dep in deps:
        lines.append(f'"{dep.name}",\n')

    return textwrap.indent("".join(lines), " " * 4)


def _generate_report(requirements_txt: pathlib.Path) -> dict:
    with tempfile.NamedTemporaryFile() as tmp:
        tmp_path = pathlib.Path(tmp.name)
        sys.argv = [
            "pip",
            "install",
            "--dry-run",
            "--ignore-installed",
            "--report",
            f"{tmp_path}",
            "-r",
            f"{requirements_txt}",
        ]
        pip_main()
        with open(tmp_path) as f:
            return json.load(f)


def _get_deps(report: dict) -> list[Dep]:
    deps = []
    for dep in report["install"]:
        try:
            dep = Dep(
                name="pypi__"
                + re.sub(
                    "[._-]+",
                    "_",
                    dep["metadata"]["name"],
                ),
                url=dep["download_info"]["url"],
                sha256=dep["download_info"]["archive_info"]["hash"][len("sha256=") :],
            )
        except:
            debug_dep = textwrap.indent(json.dumps(dep, indent=4), " " * 4)
            print(f"Could not parse the response from 'pip':\n{debug_dep}")
            raise

        deps.append(dep)

    return sorted(deps, key=lambda dep: dep.name)


def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument(
        "--start",
        type=str,
        default="# START: maintained by 'bazel run //tools/private/update_deps:update_pip_deps'",
        help="The text to match in a file when updating them.",
    )
    parser.add_argument(
        "--end",
        type=str,
        default="# END: maintained by 'bazel run //tools/private/update_deps:update_pip_deps'",
        help="The text to match in a file when updating them.",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Wether to write to files",
    )
    parser.add_argument(
        "--requirements-txt",
        type=path_from_runfiles,
        default=os.environ.get("REQUIREMENTS_TXT"),
        help="The requirements.txt path for the pypi tools, defaults to the value taken from REQUIREMENTS_TXT",
    )
    parser.add_argument(
        "--deps-bzl",
        type=path_from_runfiles,
        default=os.environ.get("DEPS_BZL"),
        help="The path for the file to be updated, defaults to the value taken from DEPS_BZL",
    )
    args = parser.parse_args()

    report = _generate_report(args.requirements_txt)
    deps = _get_deps(report)

    update_file(
        path=args.deps_bzl,
        snippet=_dep_snippet(deps),
        start_marker=args.start,
        end_marker=args.end,
        dry_run=args.dry_run,
    )


if __name__ == "__main__":
    main()
