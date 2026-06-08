"""Unit tests to show simple interactions with PyO3 modules."""

import unittest

# TODO: https://github.com/periareon/rules_venv/issues/80
from string_sum_import import sum_as_string  # type: ignore


class StringSumTest(unittest.TestCase):
    """Test Class."""

    def test_sum_as_string(self) -> None:
        """Simple test of rust defined functions."""

        result = sum_as_string(1337, 42)
        self.assertIsInstance(result, str)
        self.assertEqual("1379", result)


if __name__ == "__main__":
    unittest.main()
