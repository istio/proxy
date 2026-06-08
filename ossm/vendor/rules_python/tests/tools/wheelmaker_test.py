import io
import unittest

import tools.wheelmaker as wheelmaker


class QuoteAllFilenamesTest(unittest.TestCase):
    """Tests for quote_all_filenames behavior in _WhlFile.

    Some wheels (like torch) have all filenames quoted in their RECORD file.
    When repacking, we preserve this style to minimize diffs.
    """

    def _make_whl_file(self, quote_all: bool) -> wheelmaker._WhlFile:
        """Create a _WhlFile instance for testing."""
        buf = io.BytesIO()
        return wheelmaker._WhlFile(
            buf,
            mode="w",
            distribution_prefix="test-1.0.0",
            quote_all_filenames=quote_all,
        )

    def test_quote_all_quotes_simple_filenames(self) -> None:
        """When quote_all_filenames=True, all filenames are quoted."""
        whl = self._make_whl_file(quote_all=True)
        self.assertEqual(whl._quote_filename("foo/bar.py"), '"foo/bar.py"')

    def test_quote_all_false_leaves_simple_filenames_unquoted(self) -> None:
        """When quote_all_filenames=False, simple filenames stay unquoted."""
        whl = self._make_whl_file(quote_all=False)
        self.assertEqual(whl._quote_filename("foo/bar.py"), "foo/bar.py")

    def test_quote_all_quotes_filenames_with_commas(self) -> None:
        """Filenames with commas are always quoted, regardless of quote_all_filenames."""
        whl = self._make_whl_file(quote_all=True)
        self.assertEqual(whl._quote_filename("foo,bar/baz.py"), '"foo,bar/baz.py"')

        whl = self._make_whl_file(quote_all=False)
        self.assertEqual(whl._quote_filename("foo,bar/baz.py"), '"foo,bar/baz.py"')


class ArcNameFromTest(unittest.TestCase):
    def test_arcname_from(self) -> None:
        # (name, distribution_prefix, strip_path_prefixes, want) tuples
        checks = [
            ("a/b/c/file.py", "", [], "a/b/c/file.py"),
            ("a/b/c/file.py", "", ["a"], "/b/c/file.py"),
            ("a/b/c/file.py", "", ["a/b/"], "c/file.py"),
            # only first found is used and it's not cumulative.
            ("a/b/c/file.py", "", ["a/", "b/"], "b/c/file.py"),
            # Examples from docs
            ("foo/bar/baz/file.py", "", ["foo", "foo/bar/baz"], "/bar/baz/file.py"),
            ("foo/bar/baz/file.py", "", ["foo/bar/baz", "foo"], "/file.py"),
            ("foo/file2.py", "", ["foo/bar/baz", "foo"], "/file2.py"),
            # Files under the distribution prefix (eg mylib-1.0.0-dist-info)
            # are unmodified
            ("mylib-0.0.1-dist-info/WHEEL", "mylib", [], "mylib-0.0.1-dist-info/WHEEL"),
            ("mylib/a/b/c/WHEEL", "mylib", ["mylib"], "mylib/a/b/c/WHEEL"),
        ]
        for name, prefix, strip, want in checks:
            with self.subTest(
                name=name,
                distribution_prefix=prefix,
                strip_path_prefixes=strip,
                want=want,
            ):
                got = wheelmaker.arcname_from(
                    name=name, distribution_prefix=prefix, strip_path_prefixes=strip
                )
                self.assertEqual(got, want)


class GetNewRequirementLineTest(unittest.TestCase):
    def test_requirement(self):
        result = wheelmaker.get_new_requirement_line("requests>=2.0", "")
        self.assertEqual(result, "Requires-Dist: requests>=2.0")

    def test_requirement_and_extra(self):
        result = wheelmaker.get_new_requirement_line("requests>=2.0", "extra=='dev'")
        self.assertEqual(result, "Requires-Dist: requests>=2.0; extra=='dev'")

    def test_requirement_with_url(self):
        result = wheelmaker.get_new_requirement_line(
            "requests @ git+https://github.com/psf/requests.git@3aa6386c3", ""
        )
        self.assertEqual(
            result,
            "Requires-Dist: requests @ git+https://github.com/psf/requests.git@3aa6386c3",
        )

    def test_requirement_with_marker(self):
        result = wheelmaker.get_new_requirement_line(
            "requests>=2.0; python_version>='3.6'", ""
        )
        self.assertEqual(
            result, 'Requires-Dist: requests>=2.0; python_version >= "3.6"'
        )

    def test_requirement_with_marker_and_extra(self):
        result = wheelmaker.get_new_requirement_line(
            "requests>=2.0; python_version>='3.6'", "extra=='dev'"
        )
        self.assertEqual(
            result,
            "Requires-Dist: requests>=2.0; (python_version >= \"3.6\") and extra=='dev'",
        )


if __name__ == "__main__":
    unittest.main()
