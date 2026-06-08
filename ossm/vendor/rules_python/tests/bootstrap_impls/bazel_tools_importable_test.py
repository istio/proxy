import sys
import unittest


class BazelToolsImportableTest(unittest.TestCase):
    def test_bazel_tools_importable(self):
        try:
            import bazel_tools
            import bazel_tools.tools.python
            import bazel_tools.tools.python.runfiles
        except ImportError as exc:
            raise AssertionError(
                "Failed to import bazel_tools.python.runfiles\n"
                + "sys.path:\n"
                + "\n".join(f"{i}: {v}" for i, v in enumerate(sys.path))
            ) from exc


if __name__ == "__main__":
    unittest.main()
