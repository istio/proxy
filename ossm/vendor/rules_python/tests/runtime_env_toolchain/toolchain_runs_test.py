import json
import pathlib
import platform
import unittest

from python.runfiles import runfiles


class RunTest(unittest.TestCase):
    def test_ran(self):
        rf = runfiles.Create()
        settings_path = rf.Rlocation(
            "rules_python/tests/support/current_build_settings.json"
        )
        settings = json.loads(pathlib.Path(settings_path).read_text())
        if platform.system() == "Windows":
            self.assertEqual(
                "/_magic_pyruntime_sentinel_do_not_use", settings["interpreter_path"]
            )
        else:
            self.assertIn(
                "runtime_env_toolchain_interpreter.sh",
                settings["interpreter"]["short_path"],
            )


if __name__ == "__main__":
    unittest.main()
