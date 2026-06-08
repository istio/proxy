"""Tests importing a pyo3 extension via `imports = ["."]`."""

import unittest

from foo import bar  # type: ignore[import-untyped]


class ModulePrefixImportsTest(unittest.TestCase):
    """Test Class."""

    def test_import_and_call(self) -> None:
        """Test that a pyo3 extension can be imported via a module prefix."""

        result = bar.thing()
        self.assertEqual("hello from rust", result)


if __name__ == "__main__":
    unittest.main()
