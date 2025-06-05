# Copyright 2023 The Bazel Authors. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""
console_script generator from entry_points.txt contents.

For Python versions earlier than 3.11 and for earlier bazel versions than 7.0 we need to workaround the issue of
sys.path[0] breaking out of the runfiles tree see the following for more context:
* https://github.com/bazelbuild/rules_python/issues/382
* https://github.com/bazelbuild/bazel/pull/15701

In affected bazel and Python versions we see in programs such as `flake8`, `pylint` or `pytest` errors because the
first `sys.path` element is outside the `runfiles` directory and if the `name` of the `py_binary` is the same as
the program name, then the script (e.g. `flake8`) will start failing whilst trying to import its own internals from
the bazel entrypoint script.

The mitigation strategy is to remove the first entry in the `sys.path` if it does not have `.runfiles` and it seems
to fix the behaviour of console_scripts under `bazel run`.

This would not happen if we created a console_script binary in the root of an external repository, e.g.
`@pypi_pylint//` because the path for the external repository is already in the runfiles directory.
"""

from __future__ import annotations

import argparse
import configparser
import pathlib
import re
import sys
import textwrap

_ENTRY_POINTS_TXT = "entry_points.txt"

_TEMPLATE = """\
import sys

# See @rules_python//python/private:py_console_script_gen.py for explanation
if getattr(sys.flags, "safe_path", False):
    # We are running on Python 3.11 and we don't need this workaround
    pass
elif ".runfiles" not in sys.path[0]:
    sys.path = sys.path[1:]

try:
    from {module} import {attr}
except ImportError:
    entries = "\\n".join(sys.path)
    print("Printing sys.path entries for easier debugging:", file=sys.stderr)
    print(f"sys.path is:\\n{{entries}}", file=sys.stderr)
    raise

if __name__ == "__main__":
    sys.exit({entry_point}())
"""


class EntryPointsParser(configparser.ConfigParser):
    """A class handling entry_points.txt

    See https://packaging.python.org/en/latest/specifications/entry-points/
    """

    optionxform = staticmethod(str)


def _guess_entry_point(guess: str, console_scripts: dict[string, string]) -> str | None:
    for key, candidate in console_scripts.items():
        if guess == key:
            return candidate


def run(
    *,
    entry_points: pathlib.Path,
    out: pathlib.Path,
    console_script: str,
    console_script_guess: str,
):
    """Run the generator

    Args:
        entry_points: The entry_points.txt file to be parsed.
        out: The output file.
        console_script: The console_script entry in the entry_points.txt file.
    """
    config = EntryPointsParser()
    config.read(entry_points)

    try:
        console_scripts = dict(config["console_scripts"])
    except KeyError:
        raise RuntimeError(
            f"The package does not provide any console_scripts in its {_ENTRY_POINTS_TXT}"
        )

    if console_script:
        try:
            entry_point = console_scripts[console_script]
        except KeyError:
            available = ", ".join(sorted(console_scripts.keys()))
            raise RuntimeError(
                f"The console_script '{console_script}' was not found, only the following are available: {available}"
            ) from None
    else:
        # Get rid of the extension and the common prefix
        entry_point = _guess_entry_point(
            guess=console_script_guess,
            console_scripts=console_scripts,
        )

        if not entry_point:
            available = ", ".join(sorted(console_scripts.keys()))
            raise RuntimeError(
                f"Tried to guess that you wanted '{console_script_guess}', but could not find it. "
                f"Please select one of the following console scripts: {available}"
            ) from None

    module, _, entry_point = entry_point.rpartition(":")
    attr, _, _ = entry_point.partition(".")
    # TODO: handle 'extras' in entry_point generation
    # See https://github.com/bazelbuild/rules_python/issues/1383
    # See https://packaging.python.org/en/latest/specifications/entry-points/

    with open(out, "w") as f:
        f.write(
            _TEMPLATE.format(
                module=module,
                attr=attr,
                entry_point=entry_point,
            ),
        )


def main():
    parser = argparse.ArgumentParser(description="console_script generator")
    parser.add_argument(
        "--console-script",
        help="The console_script to generate the entry_point template for.",
    )
    parser.add_argument(
        "--console-script-guess",
        required=True,
        help="The string used for guessing the console_script if it is not provided.",
    )
    parser.add_argument(
        "entry_points",
        metavar="ENTRY_POINTS_TXT",
        type=pathlib.Path,
        help="The entry_points.txt within the dist-info of a PyPI wheel",
    )
    parser.add_argument(
        "out",
        type=pathlib.Path,
        metavar="OUT",
        help="The output file.",
    )
    args = parser.parse_args()

    run(
        entry_points=args.entry_points,
        out=args.out,
        console_script=args.console_script,
        console_script_guess=args.console_script_guess,
    )


if __name__ == "__main__":
    main()
