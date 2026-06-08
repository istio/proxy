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

"""
Regenerate a whl file after patching and cleanup the patched contents.

This script will take contents of the current directory and create a new wheel
out of it and will remove all files that were written to the wheel.
"""

from __future__ import annotations

import argparse
import csv
import difflib
import logging
import pathlib
import sys
import tempfile

from tools.wheelmaker import _WhlFile

# NOTE: Implement the following matching of what goes into the RECORD
# https://peps.python.org/pep-0491/#the-dist-info-directory
_EXCLUDES = [
    "RECORD",
    "INSTALLER",
    "RECORD.jws",
    "RECORD.p7s",
    "REQUESTED",
]

_DISTINFO = "dist-info"


def _has_all_quoted_filenames(record_contents: str) -> bool:
    """Check if all filenames in the RECORD are quoted.

    Some wheels (like torch) have all filenames quoted in their RECORD file.
    We detect this to preserve the quoting style when repacking.
    """
    lines = record_contents.splitlines()
    return all(line.startswith('"') for line in lines)


def _unidiff_output(expected, actual, record):
    """
    Helper function. Returns a string containing the unified diff of two
    multiline strings.
    """

    expected = expected.splitlines(1)
    actual = actual.splitlines(1)

    diff = difflib.unified_diff(
        expected, actual, fromfile=f"a/{record}", tofile=f"b/{record}"
    )

    return "".join(diff)


def _files_to_pack(dir: pathlib.Path, want_record: str) -> list[pathlib.Path]:
    """Check that the RECORD file entries are correct and print a unified diff on failure."""

    # First get existing files by using the RECORD file
    got_files = []
    got_distinfos = []
    for row in csv.reader(want_record.splitlines()):
        rec = row[0]
        path = dir / rec

        if not path.exists():
            # skip files that do not exist as they won't be present in the final
            # RECORD file.
            continue

        if not path.parent.name.endswith(_DISTINFO):
            got_files.append(path)
        elif path.name not in _EXCLUDES:
            got_distinfos.append(path)

    # Then get extra files present in the directory but not in the RECORD file
    extra_files = []
    extra_distinfos = []
    for path in dir.rglob("*"):
        if path.is_dir():
            continue

        elif path.parent.name.endswith(_DISTINFO):
            if path.name in _EXCLUDES:
                # NOTE: we implement the following matching of what goes into the RECORD
                # https://peps.python.org/pep-0491/#the-dist-info-directory
                continue
            elif path not in got_distinfos:
                extra_distinfos.append(path)

        elif path not in got_files:
            extra_files.append(path)

    # sort the extra files for reproducibility
    extra_files.sort()
    extra_distinfos.sort()

    # This order ensures that the structure of the RECORD file is always the
    # same and ensures smaller patchsets to the RECORD file in general
    return got_files + extra_files + got_distinfos + extra_distinfos


def main(sys_argv):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "whl_path",
        type=pathlib.Path,
        help="The original wheel file that we have patched.",
    )
    parser.add_argument(
        "--record-patch",
        type=pathlib.Path,
        help="The output path that we are going to write the RECORD file patch to.",
    )
    parser.add_argument(
        "output",
        type=pathlib.Path,
        help="The output path that we are going to write a new file to.",
    )
    args = parser.parse_args(sys_argv)

    cwd = pathlib.Path.cwd()
    logging.debug("=" * 80)
    logging.debug("Repackaging the wheel")
    logging.debug("=" * 80)

    with tempfile.TemporaryDirectory(dir=cwd) as tmpdir:
        patched_wheel_dir = cwd / tmpdir
        logging.debug(f"Created a tmpdir: {patched_wheel_dir}")

        excludes = [args.whl_path, patched_wheel_dir]

        logging.debug("Moving whl contents to the newly created tmpdir")
        for p in cwd.glob("*"):
            if p in excludes:
                logging.debug(f"Ignoring: {p}")
                continue

            rel_path = p.relative_to(cwd)
            dst = p.rename(patched_wheel_dir / rel_path)
            logging.debug(f"mv {p} -> {dst}")

        distinfo_dir = next(iter(patched_wheel_dir.glob("*dist-info")))
        logging.debug(f"Found dist-info dir: {distinfo_dir}")
        record_path = distinfo_dir / "RECORD"
        record_contents = record_path.read_text() if record_path.exists() else ""
        quote_files = _has_all_quoted_filenames(record_contents)
        distribution_prefix = distinfo_dir.with_suffix("").name

        with _WhlFile(
            args.output,
            mode="w",
            distribution_prefix=distribution_prefix,
            quote_all_filenames=quote_files,
        ) as out:
            for p in _files_to_pack(patched_wheel_dir, record_contents):
                rel_path = p.relative_to(patched_wheel_dir)
                out.add_file(str(rel_path), p)

            logging.debug(f"Writing RECORD file")
            got_record = out.add_recordfile()

    if got_record == record_contents:
        logging.info(f"Created a whl file: {args.output}")
        return

    record_diff = _unidiff_output(
        record_contents,
        got_record,
        out.distinfo_path("RECORD"),
    )
    args.record_patch.write_text(record_diff)
    logging.warning(
        f"Please apply patch to the RECORD file ({args.record_patch}):\n{record_diff}"
    )


if __name__ == "__main__":
    logging.basicConfig(
        format="%(module)s: %(levelname)s: %(message)s", level=logging.DEBUG
    )

    sys.exit(main(sys.argv[1:]))
