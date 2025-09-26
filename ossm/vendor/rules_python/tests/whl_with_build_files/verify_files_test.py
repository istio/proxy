import unittest


class VerifyFilestest(unittest.TestCase):

    def test_wheel_with_build_files_importable(self):
        # If the BUILD files are present, then these imports should fail
        # because globs won't pass package boundaries, and the necessary
        # py files end up missing in runfiles.
        import somepkg
        import somepkg.a
        import somepkg.subpkg
        import somepkg.subpkg.b


if __name__ == "__main__":
    unittest.main()
