import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from typing import Iterable

from python import runfiles

rfiles = runfiles.Create()

# Signals the tests below whether we should be expecting the import of
# helpers/test_module.py on the REPL to work or not.
EXPECT_TEST_MODULE_IMPORTABLE = os.environ["EXPECT_TEST_MODULE_IMPORTABLE"] == "1"


# An arbitrary piece of code that sets some kind of variable. The variable needs to persist into the
# actual shell.
PYTHONSTARTUP_SETS_VAR = """\
foo = 1234
"""


class ReplTest(unittest.TestCase):
    def setUp(self):
        self.repl = rfiles.Rlocation("rules_python/python/bin/repl")
        assert self.repl

    def run_code_in_repl(self, lines: Iterable[str], *, env=None) -> str:
        """Runs the lines of code in the REPL and returns the text output."""
        try:
            return subprocess.check_output(
                [self.repl],
                text=True,
                stderr=subprocess.STDOUT,
                input="\n".join(lines),
                env=env,
            ).strip()
        except subprocess.CalledProcessError as error:
            raise RuntimeError(f"Failed to run the REPL:\n{error.stdout}") from error

    def test_repl_version(self):
        """Validates that we can successfully execute arbitrary code on the REPL."""

        result = self.run_code_in_repl(
            [
                "import sys",
                "v = sys.version_info",
                "print(f'version: {v.major}.{v.minor}')",
            ]
        )
        self.assertIn("version: 3.12", result)

    def test_cannot_import_test_module_directly(self):
        """Validates that we cannot import helper/test_module.py since it's not a direct dep."""
        with self.assertRaises(ModuleNotFoundError):
            import test_module

    @unittest.skipIf(
        not EXPECT_TEST_MODULE_IMPORTABLE, "test only works without repl_dep set"
    )
    def test_import_test_module_success(self):
        """Validates that we can import helper/test_module.py when repl_dep is set."""
        result = self.run_code_in_repl(
            [
                "import test_module",
                "test_module.print_hello()",
            ]
        )
        self.assertIn("Hello World", result)

    @unittest.skipIf(
        EXPECT_TEST_MODULE_IMPORTABLE, "test only works without repl_dep set"
    )
    def test_import_test_module_failure(self):
        """Validates that we cannot import helper/test_module.py when repl_dep isn't set."""
        result = self.run_code_in_repl(
            [
                "import test_module",
            ]
        )
        self.assertIn("ModuleNotFoundError: No module named 'test_module'", result)

    def test_pythonstartup_gets_executed(self):
        """Validates that we can use the variables from PYTHONSTARTUP in the console itself."""
        with tempfile.TemporaryDirectory() as tempdir:
            pythonstartup = Path(tempdir) / "pythonstartup.py"
            pythonstartup.write_text(PYTHONSTARTUP_SETS_VAR)

            env = os.environ.copy()
            env["PYTHONSTARTUP"] = str(pythonstartup)

            result = self.run_code_in_repl(
                [
                    "print(f'The value of foo is {foo}')",
                ],
                env=env,
            )

        self.assertIn("The value of foo is 1234", result)

    def test_pythonstartup_doesnt_leak(self):
        """Validates that we don't accidentally leak code into the console.

        This test validates that a few of the variables we use in the template and stub are not
        accessible in the REPL itself.
        """
        with tempfile.TemporaryDirectory() as tempdir:
            pythonstartup = Path(tempdir) / "pythonstartup.py"
            pythonstartup.write_text(PYTHONSTARTUP_SETS_VAR)

            env = os.environ.copy()
            env["PYTHONSTARTUP"] = str(pythonstartup)

            for var_name in ("exitmsg", "sys", "code", "bazel_runfiles", "STUB_PATH"):
                with self.subTest(var_name=var_name):
                    result = self.run_code_in_repl([f"print({var_name})"], env=env)
                    self.assertIn(
                        f"NameError: name '{var_name}' is not defined", result
                    )


if __name__ == "__main__":
    unittest.main()
