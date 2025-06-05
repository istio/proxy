"""A CLI to evaluate env markers for requirements files.

A simple script to evaluate the `requirements.txt` files. Currently it is only
handling environment markers in the requirements files, but in the future it
may handle more things. We require a `python` interpreter that can run on the
host platform and then we depend on the [packaging] PyPI wheel.

In order to be able to resolve requirements files for any platform, we are
re-using the same code that is used in the `whl_library` installer. See
[here](../whl_installer/wheel.py).

Requirements for the code are:
- Depends only on `packaging` and core Python.
- Produces the same result irrespective of the Python interpreter platform or version.

[packaging]: https://packaging.pypa.io/en/stable/
"""

import argparse
import json
import pathlib

from packaging.requirements import Requirement

from python.private.pypi.whl_installer.platform import Platform

INPUT_HELP = """\
Input path to read the requirements as a json file, the keys in the dictionary
are the requirements lines and the values are strings of target platforms.
"""
OUTPUT_HELP = """\
Output to write the requirements as a json filepath, the keys in the dictionary
are the requirements lines and the values are strings of target platforms, which
got changed based on the evaluated markers.
"""


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input_path", type=pathlib.Path, help=INPUT_HELP.strip())
    parser.add_argument("output_path", type=pathlib.Path, help=OUTPUT_HELP.strip())
    args = parser.parse_args()

    with args.input_path.open() as f:
        reqs = json.load(f)

    response = {}
    for requirement_line, target_platforms in reqs.items():
        entry, prefix, hashes = requirement_line.partition("--hash")
        hashes = prefix + hashes

        req = Requirement(entry)
        for p in target_platforms:
            (platform,) = Platform.from_string(p)
            if not req.marker or req.marker.evaluate(platform.env_markers("")):
                response.setdefault(requirement_line, []).append(p)

    with args.output_path.open("w") as f:
        json.dump(response, f)


if __name__ == "__main__":
    main()
