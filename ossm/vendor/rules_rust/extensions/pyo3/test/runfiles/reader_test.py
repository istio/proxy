"""A test that demonstrates rust code being able to interface with runfiles."""

import unittest
from pathlib import Path
from test.runfiles import reader

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


class RunfilesTest(unittest.TestCase):
    """Test Class."""

    def test_reader(self) -> None:
        """A test which uses runfile data from rust code."""

        result = reader.read_data()
        self.assertIsInstance(result, str)
        self.assertEqual("La-Li-Lu-Le-Lo", result.strip())

    def test_transitive_runfiles_access(self) -> None:
        """A test which interacts with transitive rust runfiles."""

        runfiles = Runfiles.Create()
        if not runfiles:
            raise EnvironmentError("Failed to locate runfiles.")

        rlocationpath = "rules_rust_pyo3/test/runfiles/data.txt"
        data_file = _rlocation(runfiles, rlocationpath)

        self.assertEqual(
            "La-Li-Lu-Le-Lo", data_file.read_text(encoding="utf-8").strip()
        )


if __name__ == "__main__":
    unittest.main()
