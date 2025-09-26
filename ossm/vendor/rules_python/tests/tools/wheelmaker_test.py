import unittest

import tools.wheelmaker as wheelmaker


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


if __name__ == "__main__":
    unittest.main()
