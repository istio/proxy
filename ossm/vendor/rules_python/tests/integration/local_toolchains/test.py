import shutil
import subprocess
import sys
import unittest


class LocalToolchainTest(unittest.TestCase):
    maxDiff = None

    def test_python_from_path_used(self):
        shell_path = shutil.which("python3")

        # We call the interpreter and print its executable because of
        # things like pyenv: they install a shim that re-execs python.
        # The shim is e.g. /home/user/.pyenv/shims/python3, which then
        # runs e.g. /usr/bin/python3
        expected = subprocess.check_output(
            [shell_path, "-c", "import sys; print(sys.executable)"],
            text=True,
        )
        expected = expected.strip().lower()
        # Normalize case: Windows may have case differences
        self.assertEqual(expected.lower(), sys.executable.lower())


if __name__ == "__main__":
    unittest.main()
