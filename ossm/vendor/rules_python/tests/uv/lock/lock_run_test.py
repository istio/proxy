import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

from python import runfiles

rfiles = runfiles.Create()


def _relative_rpath(path: str) -> Path:
    p = (Path("_main") / "tests" / "uv" / "lock" / path).as_posix()
    rpath = rfiles.Rlocation(p)
    if not rpath:
        raise ValueError(f"Could not find file: {p}")

    return Path(rpath)


class LockTests(unittest.TestCase):
    def test_requirements_updating_for_the_first_time(self):
        # Given
        copier_path = _relative_rpath("requirements_new_file.update")

        # When
        with tempfile.TemporaryDirectory() as dir:
            workspace_dir = Path(dir)
            want_path = workspace_dir / "tests" / "uv" / "lock" / "does_not_exist.txt"

            self.assertFalse(
                want_path.exists(), "The path should not exist after the test"
            )
            output = subprocess.run(
                copier_path,
                capture_output=True,
                env={
                    "BUILD_WORKSPACE_DIRECTORY": f"{workspace_dir}",
                },
            )

            # Then
            self.assertEqual(0, output.returncode, output.stderr)
            self.assertIn(
                "cp <bazel-sandbox>/tests/uv/lock/requirements_new_file",
                output.stdout.decode("utf-8"),
            )
            self.assertTrue(want_path.exists(), "The path should exist after the test")
            self.assertNotEqual(want_path.read_text(), "")

    def test_requirements_updating(self):
        # Given
        copier_path = _relative_rpath("requirements.update")
        existing_file = _relative_rpath("testdata/requirements.txt")
        want_text = existing_file.read_text()

        # When
        with tempfile.TemporaryDirectory() as dir:
            workspace_dir = Path(dir)
            want_path = (
                workspace_dir
                / "tests"
                / "uv"
                / "lock"
                / "testdata"
                / "requirements.txt"
            )
            want_path.parent.mkdir(parents=True)
            want_path.write_text(
                want_text + "\n\n"
            )  # Write something else to see that it is restored

            output = subprocess.run(
                copier_path,
                capture_output=True,
                env={
                    "BUILD_WORKSPACE_DIRECTORY": f"{workspace_dir}",
                },
            )

            # Then
            self.assertEqual(0, output.returncode)
            self.assertIn(
                "cp <bazel-sandbox>/tests/uv/lock/requirements",
                output.stdout.decode("utf-8"),
            )
            self.assertEqual(want_path.read_text(), want_text)

    def test_requirements_run_on_the_first_time(self):
        # Given
        copier_path = _relative_rpath("requirements_new_file.run")

        # When
        with tempfile.TemporaryDirectory() as dir:
            workspace_dir = Path(dir)
            want_path = workspace_dir / "tests" / "uv" / "lock" / "does_not_exist.txt"
            # NOTE @aignas 2025-03-18: right now we require users to have the folder
            # there already
            want_path.parent.mkdir(parents=True)

            self.assertFalse(
                want_path.exists(), "The path should not exist after the test"
            )
            output = subprocess.run(
                copier_path,
                capture_output=True,
                env={
                    "BUILD_WORKSPACE_DIRECTORY": f"{workspace_dir}",
                },
            )

            # Then
            self.assertEqual(0, output.returncode, output.stderr)
            self.assertTrue(want_path.exists(), "The path should exist after the test")
            got_contents = want_path.read_text()
            self.assertNotEqual(got_contents, "")
            self.assertIn(
                got_contents,
                output.stdout.decode("utf-8"),
            )

    def test_requirements_run(self):
        # Given
        copier_path = _relative_rpath("requirements.run")
        existing_file = _relative_rpath("testdata/requirements.txt")
        want_text = existing_file.read_text()

        # When
        with tempfile.TemporaryDirectory() as dir:
            workspace_dir = Path(dir)
            want_path = (
                workspace_dir
                / "tests"
                / "uv"
                / "lock"
                / "testdata"
                / "requirements.txt"
            )

            want_path.parent.mkdir(parents=True)
            want_path.write_text(
                want_text + "\n\n"
            )  # Write something else to see that it is restored

            output = subprocess.run(
                copier_path,
                capture_output=True,
                env={
                    "BUILD_WORKSPACE_DIRECTORY": f"{workspace_dir}",
                },
            )

            # Then
            self.assertEqual(0, output.returncode, output.stderr)
            self.assertTrue(want_path.exists(), "The path should exist after the test")
            got_contents = want_path.read_text()
            self.assertNotEqual(got_contents, "")
            self.assertIn(
                got_contents,
                output.stdout.decode("utf-8"),
            )


if __name__ == "__main__":
    unittest.main()
