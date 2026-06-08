"""Tests that a pyo3 extension can be imported via a module prefix."""

import unittest
from test.module_prefix.foo import bar


class ModulePrefixImportTest(unittest.TestCase):
    """Test Class."""

    def test_import_and_call(self) -> None:
        """Test that a pyo3 extension can be imported via a module prefix."""

        result = bar.thing()
        self.assertEqual("hello from rust", result)


if __name__ == "__main__":
    unittest.main()
