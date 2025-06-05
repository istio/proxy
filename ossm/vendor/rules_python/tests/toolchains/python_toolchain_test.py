import json
import os
import pathlib
import pprint
import sys
import unittest

from python.runfiles import runfiles


class PythonToolchainTest(unittest.TestCase):
    def test_expected_toolchain_matches(self):
        expect_version = os.environ["EXPECT_PYTHON_VERSION"]

        rf = runfiles.Create()
        settings_path = rf.Rlocation(
            "rules_python/tests/support/current_build_settings.json"
        )
        settings = json.loads(pathlib.Path(settings_path).read_text())

        expected = "python_{}".format(expect_version.replace(".", "_"))
        msg = (
            "Expected toolchain not found\n"
            + f"Expected toolchain label to contain: {expected}\n"
            + "Actual build settings:\n"
            + pprint.pformat(settings)
        )
        self.assertIn(expected, settings["toolchain_label"], msg)

        actual = "{v.major}.{v.minor}.{v.micro}".format(v=sys.version_info)
        self.assertEqual(actual, expect_version)


if __name__ == "__main__":
    unittest.main()
