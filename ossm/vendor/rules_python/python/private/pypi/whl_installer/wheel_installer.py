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

"""Build and/or fetch a single wheel based on the requirement passed in"""

import errno
import glob
import json
import os
import re
import subprocess
import sys
from pathlib import Path
from tempfile import NamedTemporaryFile
from typing import Dict, List, Optional, Set, Tuple

from pip._vendor.packaging.utils import canonicalize_name

from python.private.pypi.whl_installer import arguments, wheel


def _configure_reproducible_wheels() -> None:
    """Modifies the environment to make wheel building reproducible.
    Wheels created from sdists are not reproducible by default. We can however workaround this by
    patching in some configuration with environment variables.
    """

    # wheel, by default, enables debug symbols in GCC. This incidentally captures the build path in the .so file
    # We can override this behavior by disabling debug symbols entirely.
    # https://github.com/pypa/pip/issues/6505
    if "CFLAGS" in os.environ:
        os.environ["CFLAGS"] += " -g0"
    else:
        os.environ["CFLAGS"] = "-g0"

    # set SOURCE_DATE_EPOCH to 1980 so that we can use python wheels
    # https://github.com/NixOS/nixpkgs/blob/master/doc/languages-frameworks/python.section.md#python-setuppy-bdist_wheel-cannot-create-whl
    if "SOURCE_DATE_EPOCH" not in os.environ:
        os.environ["SOURCE_DATE_EPOCH"] = "315532800"

    # Python wheel metadata files can be unstable.
    # See https://bitbucket.org/pypa/wheel/pull-requests/74/make-the-output-of-metadata-files/diff
    if "PYTHONHASHSEED" not in os.environ:
        os.environ["PYTHONHASHSEED"] = "0"


def _parse_requirement_for_extra(
    requirement: str,
) -> Tuple[Optional[str], Optional[Set[str]]]:
    """Given a requirement string, returns the requirement name and set of extras, if extras specified.
    Else, returns (None, None)
    """

    # https://www.python.org/dev/peps/pep-0508/#grammar
    extras_pattern = re.compile(
        r"^\s*([0-9A-Za-z][0-9A-Za-z_.\-]*)\s*\[\s*([0-9A-Za-z][0-9A-Za-z_.\-]*(?:\s*,\s*[0-9A-Za-z][0-9A-Za-z_.\-]*)*)\s*\]"
    )

    matches = extras_pattern.match(requirement)
    if matches:
        return (
            canonicalize_name(matches.group(1)),
            {extra.strip() for extra in matches.group(2).split(",")},
        )

    return None, None


def _extract_wheel(
    wheel_file: str,
    extras: Dict[str, Set[str]],
    enable_pipstar: bool,
    platforms: List[wheel.Platform],
    installation_dir: Path = Path("."),
) -> None:
    """Extracts wheel into given directory and creates py_library and filegroup targets.

    Args:
        wheel_file: the filepath of the .whl
        installation_dir: the destination directory for installation of the wheel.
        extras: a list of extras to add as dependencies for the installed wheel
        enable_pipstar: if true, turns off certain operations.
    """

    whl = wheel.Wheel(wheel_file)
    whl.unzip(installation_dir)

    if enable_pipstar:
        return

    extras_requested = extras[whl.name] if whl.name in extras else set()
    dependencies = whl.dependencies(extras_requested, platforms)

    metadata = {
        "name": whl.name,
        "version": whl.version,
        "deps": dependencies.deps,
        "deps_by_platform": dependencies.deps_select,
        "entry_points": [
            {
                "name": name,
                "module": module,
                "attribute": attribute,
            }
            for name, (module, attribute) in sorted(whl.entry_points().items())
        ],
    }

    with open(os.path.join(installation_dir, "metadata.json"), "w") as f:
        json.dump(metadata, f)


def main() -> None:
    args = arguments.parser(description=__doc__).parse_args()
    deserialized_args = dict(vars(args))
    arguments.deserialize_structured_args(deserialized_args)

    _configure_reproducible_wheels()

    if args.whl_file:
        whl = Path(args.whl_file)

        name, extras_for_pkg = _parse_requirement_for_extra(args.requirement)
        extras = {name: extras_for_pkg} if extras_for_pkg and name else dict()
        _extract_wheel(
            wheel_file=whl,
            extras=extras,
            enable_pipstar=args.enable_pipstar,
            platforms=arguments.get_platforms(args),
        )
        return

    pip_args = (
        [sys.executable, "-m", "pip"]
        + (["--isolated"] if args.isolated else [])
        + (["download", "--only-binary=:all:"] if args.download_only else ["wheel"])
        + ["--no-deps"]
        + deserialized_args["extra_pip_args"]
    )

    requirement_file = NamedTemporaryFile(mode="wb", delete=False)
    try:
        requirement_file.write(args.requirement.encode("utf-8"))
        requirement_file.flush()
        # Close the file so pip is allowed to read it when running on Windows.
        # For more information, see: https://bugs.python.org/issue14243
        requirement_file.close()
        # Requirement specific args like --hash can only be passed in a requirements file,
        # so write our single requirement into a temp file in case it has any of those flags.
        pip_args.extend(["-r", requirement_file.name])

        env = os.environ.copy()
        env.update(deserialized_args["environment"])
        # Assumes any errors are logged by pip so do nothing. This command will fail if pip fails
        subprocess.run(pip_args, check=True, env=env)
    finally:
        try:
            os.unlink(requirement_file.name)
        except OSError as e:
            if e.errno != errno.ENOENT:
                raise

    whl = Path(next(iter(glob.glob("*.whl"))))

    with open("whl_file.json", "w") as f:
        json.dump({"whl_file": f"{whl.resolve()}"}, f)


if __name__ == "__main__":
    main()
