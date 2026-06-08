"""Test a pyo3 extension module in a venv."""

import os
import platform
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

from python.runfiles import Runfiles


def _rlocation(runfiles: Runfiles, rlocationpath: str) -> Path:
    """Look up a runfile and ensure the file exists

    Args:
        runfiles: The runfiles object
        rlocationpath: The runfile key

    Returns:
        The requested runifle.
    """
    runfile = runfiles.Rlocation(rlocationpath)
    if not runfile:
        raise FileNotFoundError(f"Failed to find runfile: {rlocationpath}")
    path = Path(runfile)
    if not path.exists():
        raise FileNotFoundError(f"Runfile does not exist: ({rlocationpath}) {path}")
    return path


def _create_venv(path: Path) -> Path:
    """Create a venv in the given path and return the new interpreter."""
    subprocess.run(
        [
            sys.executable,
            "-m",
            "venv",
            path,
        ],
        check=True,
    )

    if platform.system() == "Windows":
        return path / "Scripts/python.exe"

    return path / "bin/python"


class VenvTest(unittest.TestCase):
    """PyO3 tests within a venv."""

    def test_wheel_in_venv(self) -> None:
        """A test that shows pyo3 extensions are usable from wheels

        This covers the published/canonical workflow for python users.
        """
        runfiles = Runfiles.Create()
        if not runfiles:
            raise EnvironmentError("Failed to locate runfiles.")

        rust_wheel = _rlocation(runfiles, os.environ["RUST_WHEEL"])

        with tempfile.TemporaryDirectory(dir=os.getenv("TEST_TMPDIR")) as tmp_dir:
            tmp_path = Path(tmp_dir)
            venv_interp = _create_venv(tmp_path / "venv")

            # Install the wheel
            subprocess.run(
                [
                    venv_interp,
                    "-m",
                    "pip",
                    "--disable-pip-version-check",
                    "install",
                    rust_wheel,
                ],
                check=True,
            )

            output = tmp_path / "output.txt"
            import_str = os.environ["IMPORT_STR"]

            # Run code which uses the wheel
            subprocess.run(
                [
                    venv_interp,
                    "-c",
                    "; ".join(
                        [
                            import_str,
                            "from pathlib import Path",
                            f'Path("{output.as_posix()}").write_text(sum_as_string(1337, 42), encoding="utf-8")',
                        ]
                    ),
                ],
                check=True,
            )

            self.assertEqual(output.read_text(encoding="utf-8"), "1379")


if __name__ == "__main__":
    unittest.main()
