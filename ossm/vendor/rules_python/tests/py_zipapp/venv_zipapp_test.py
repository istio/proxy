import contextlib
import os
import subprocess
import unittest
import zipfile


class PyZipAppTest(unittest.TestCase):
    def test_zipapp_runnable(self):
        zipapp_path = os.environ["TEST_ZIPAPP"]

        try:
            output = (
                subprocess.check_output([zipapp_path], stderr=subprocess.STDOUT)
                .decode("utf-8")
                .strip()
            )
        except subprocess.CalledProcessError as e:
            self.fail(
                (
                    "exec failed: {}\n"
                    + "exit code: {}\n"
                    + "=== stdout/stderr start ===\n"
                    "{}\n" + "=== stdout/stderr end ==="
                ).format(zipapp_path, e.returncode, e.output.decode("utf-8"))
            )
        self.assertIn("Hello from zipapp", output)
        self.assertIn("dep:", output)

    def assertHasPathMatchingSuffix(self, namelist, suffix, msg=None):
        if not any(name.endswith(suffix) for name in namelist):
            self.fail(
                (msg or f"No path in zipapp matching suffix '{suffix}'")
                + "\nAvailable paths:\n"
                + "\n".join(namelist)
            )

    def assertZipEntryIsSymlink(self, zip_file, path, msg=None):
        try:
            info = zip_file.getinfo(path)
        except KeyError:
            self.fail(msg or f"Path '{path}' not found in zipfile")

        # S_IFLNK is 0o120000.
        # ZipInfo.external_attr is 32 bits: the high 16 bits are Unix attributes.
        is_symlink = (info.external_attr >> 16) & 0o170000 == 0o120000
        if not is_symlink:
            self.fail(msg or f"Path '{path}' is not a symlink")

    def _is_bzlmod_enabled(self):
        return os.environ["BZLMOD_ENABLED"] == "1"

    @contextlib.contextmanager
    def _open_zipapp(self, path):
        zf = None
        try:
            try:
                zf = zipfile.ZipFile(path, "r")
            except zipfile.BadZipFile:
                # On windows, the main output is the launcher .exe file, and the
                # zip file is a sibling file.
                path = path.replace(".exe", ".zip")
                zf = zipfile.ZipFile(path, "r")
            if zf:
                yield zf
        finally:
            if zf:
                zf.close()

    def test_zipapp_structure(self):
        zipapp_path = os.environ["TEST_ZIPAPP"]

        with self._open_zipapp(zipapp_path) as zf:
            namelist = zf.namelist()

            if self._is_bzlmod_enabled():
                self.assertIn("runfiles/_repo_mapping", namelist)

            self.assertHasPathMatchingSuffix(namelist, "/pyvenv.cfg")

            # The venv directory name depends on the target name, so find it
            # by looking for pyvenv.cfg.
            venv_config = next(
                (name for name in namelist if name.endswith("/pyvenv.cfg")), None
            )
            self.assertIsNotNone(venv_config)

            venv_root = os.path.dirname(venv_config)

            # Verify bin/python3 exists and is a symlink
            python_bin = f"{venv_root}/bin/python3"
            self.assertZipEntryIsSymlink(zf, python_bin)

            # Verify _bazel_site_init.py exists in site-packages
            self.assertHasPathMatchingSuffix(
                namelist, "/site-packages/_bazel_site_init.py"
            )


if __name__ == "__main__":
    unittest.main()
