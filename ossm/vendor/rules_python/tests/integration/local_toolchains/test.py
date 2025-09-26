import os.path
import shutil
import subprocess
import sys
import tempfile
import unittest


class LocalToolchainTest(unittest.TestCase):
    maxDiff = None

    def test_python_from_path_used(self):
        # NOTE: This is a bit brittle. It assumes the environment during the
        # repo-phase and when the test is run are roughly the same. It's
        # easy to violate this condition if there are shell-local changes
        # that wouldn't be reflected when sub-shells are run later.
        shell_path = shutil.which("python3")

        # We call the interpreter and print its executable because of
        # things like pyenv: they install a shim that re-execs python.
        # The shim is e.g. /home/user/.pyenv/shims/python3, which then
        # runs e.g. /usr/bin/python3
        with tempfile.TemporaryDirectory() as temp_dir:
            file_path = os.path.join(temp_dir, "info.py")
            with open(file_path, 'w') as f:
                f.write(
                """
import sys
print(sys.executable)
print(sys._base_executable)
"""
                )
                f.flush()
            output_lines = (
                subprocess.check_output(
                    [shell_path, file_path],
                    text=True,
                )
                .strip()
                .splitlines()
            )
        shell_exe, shell_base_exe = output_lines

        # Call realpath() to help normalize away differences from symlinks.
        # Use base executable to ignore a venv the test may be running within.
        expected = os.path.realpath(shell_base_exe.strip().lower())
        actual = os.path.realpath(sys._base_executable.lower())

        msg = f"""
details of executables:
test's runtime:
{sys.executable=}
{sys._base_executable=}
realpath exe     : {os.path.realpath(sys.executable)}
realpath base_exe: {os.path.realpath(sys._base_executable)}

from shell resolution:
which python3: {shell_path=}:
{shell_exe=}
{shell_base_exe=}
realpath exe     : {os.path.realpath(shell_exe)}
realpath base_exe: {os.path.realpath(shell_base_exe)}
""".strip()

        # Normalize case: Windows may have case differences
        self.assertEqual(expected.lower(), actual.lower(), msg=msg)


if __name__ == "__main__":
    unittest.main()
