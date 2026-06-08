import unittest

from python.private.pypi import repack_whl


class HasAllQuotedFilenamesTest(unittest.TestCase):
    """Tests for _has_all_quoted_filenames detection logic."""

    def test_all_quoted(self) -> None:
        """Returns True when all lines start with quotes (torch-style)."""
        record = """\
"torch/__init__.py",sha256=abc,123
"torch/utils.py",sha256=def,456
"torch-2.0.0.dist-info/WHEEL",sha256=ghi,789
"""
        self.assertTrue(repack_whl._has_all_quoted_filenames(record))

    def test_none_quoted(self) -> None:
        """Returns False when no lines are quoted (standard style)."""
        record = """\
torch/__init__.py,sha256=abc,123
torch/utils.py,sha256=def,456
torch-2.0.0.dist-info/WHEEL,sha256=ghi,789
"""
        self.assertFalse(repack_whl._has_all_quoted_filenames(record))

    def test_mixed_quoting(self) -> None:
        """Returns False when only some lines are quoted."""
        record = """\
"file,with,commas.py",sha256=abc,123
normal_file.py,sha256=def,456
"""
        self.assertFalse(repack_whl._has_all_quoted_filenames(record))


if __name__ == "__main__":
    unittest.main()
