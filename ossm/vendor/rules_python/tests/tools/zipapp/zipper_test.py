import os
import pathlib
import shutil
import tempfile
import time
import unittest
import zipfile

from tools.private.zipapp import zipper


class ZipperTest(unittest.TestCase):
    def setUp(self):
        self.test_dir = pathlib.Path(tempfile.mkdtemp())
        self.manifest_path = self.test_dir / "manifest.txt"
        self.output_zip = self.test_dir / "output.zip"

    def tearDown(self):
        shutil.rmtree(self.test_dir)

    def _create_zip(self, **kwargs):
        defaults = {
            "manifest_path": self.manifest_path,
            "output_zip": self.output_zip,
            "compress_level": 0,
            "workspace_name": "my_ws",
            "legacy_external_runfiles": False,
            "runfiles_dir": "runfiles",
        }
        defaults.update(kwargs)
        zipper.create_zip(**defaults)

    def assertZipFileContent(
        self, zf, path, content=None, is_symlink=False, target=None
    ):
        info = zf.getinfo(path)
        if is_symlink:
            self.assertTrue(
                self.is_symlink(info),
                f"{path} should be a symlink but is not",
            )
            self.assertEqual(zf.read(path).decode(), target)
        else:
            self.assertFalse(
                self.is_symlink(info),
                f"{path} should NOT be a symlink but is",
            )
            self.assertEqual(zf.read(path).decode(), content)

    def test_create_zip_with_files_and_symlinks(self):
        file1_path = self.test_dir / "file1.txt"
        file1_path.write_text("content1")

        link_target_path = "target.txt"  # Relative target
        symlink_path = self.test_dir / "symlink_source"
        symlink_path.symlink_to(link_target_path)

        manifest_content = [
            f"regular|0|file1.txt|{file1_path}",
            f"rf-file|0|foo/bar.txt|{file1_path}",
            f"rf-symlink|1|link1|{symlink_path}",  # Should read target 'target.txt'
            f"rf-root-symlink|0|root_file|{file1_path}",
            f"rf-empty|empty_file",
        ]
        self.manifest_path.write_text("\n".join(manifest_content))

        self._create_zip()

        self.assertTrue(self.output_zip.exists())

        with zipfile.ZipFile(self.output_zip, "r") as zf:
            self.assertEqual(
                set(zf.namelist()),
                {
                    "file1.txt",
                    "runfiles/my_ws/foo/bar.txt",
                    "runfiles/my_ws/link1",
                    "runfiles/root_file",
                    "runfiles/my_ws/empty_file",
                },
            )

            self.assertZipFileContent(zf, "file1.txt", content="content1")
            self.assertZipFileContent(
                zf, "runfiles/my_ws/foo/bar.txt", content="content1"
            )
            self.assertZipFileContent(
                zf, "runfiles/my_ws/link1", is_symlink=True, target="target.txt"
            )
            self.assertZipFileContent(zf, "runfiles/root_file", content="content1")
            self.assertZipFileContent(zf, "runfiles/my_ws/empty_file", content="")

    def test_timestamps_are_deterministic(self):
        # Create a content file with a specific recent timestamp
        file1_path = self.test_dir / "file1.txt"
        file1_path.write_text("content1")

        # Set mtime to something recent (e.g. now)
        os.utime(file1_path, None)

        manifest_content = [
            f"regular|0|file1.txt|{file1_path}",
        ]

        self.manifest_path.write_text("\n".join(manifest_content))

        self._create_zip()

        with zipfile.ZipFile(self.output_zip, "r") as zf:
            info = zf.getinfo("file1.txt")
            # DOS epoch is 1980-01-01 00:00:00
            expected_date_time = (1980, 1, 1, 0, 0, 0)
            self.assertEqual(info.date_time, expected_date_time)

    def test_runfiles_mapping_with_cross_repo_paths(self):
        # Create content file
        file1_path = self.test_dir / "file1.txt"
        file1_path.write_text("content1")

        manifest_content = [
            f"rf-file|0|../other_repo/foo.txt|{file1_path}",
            f"rf-empty|../other_repo/empty_file",
        ]

        self.manifest_path.write_text("\n".join(manifest_content))

        self._create_zip(workspace_name="my_ws")

        with zipfile.ZipFile(self.output_zip, "r") as zf:
            self.assertEqual(
                set(zf.namelist()),
                {
                    "runfiles/other_repo/foo.txt",
                    "runfiles/other_repo/empty_file",
                },
            )
            self.assertZipFileContent(
                zf, "runfiles/other_repo/foo.txt", content="content1"
            )
            self.assertZipFileContent(zf, "runfiles/other_repo/empty_file", content="")

    def test_runfiles_mapping_with_legacy_external_paths(self):
        file1_path = self.test_dir / "file1.txt"
        file1_path.write_text("content1")

        manifest_content = [
            f"rf-file|0|external/other_repo/foo.txt|{file1_path}",
            f"rf-empty|external/other_repo/empty_file",
        ]

        self.manifest_path.write_text("\n".join(manifest_content))

        self._create_zip(workspace_name="my_ws", legacy_external_runfiles=True)

        with zipfile.ZipFile(self.output_zip, "r") as zf:
            self.assertEqual(
                set(zf.namelist()),
                {
                    "runfiles/other_repo/foo.txt",
                    "runfiles/other_repo/empty_file",
                },
            )
            self.assertZipFileContent(
                zf, "runfiles/other_repo/foo.txt", content="content1"
            )
            self.assertZipFileContent(zf, "runfiles/other_repo/empty_file", content="")

    def test_output_deterministic(self):
        # Create files
        file1 = self.test_dir / "file1"
        file1.write_text("1")
        file2 = self.test_dir / "file2"
        file2.write_text("2")
        file3 = self.test_dir / "file3"
        file3.write_text("3")

        # Manifest entries mixed up
        # We want the final order to be:
        # 1. a/regular (regular)
        # 2. runfiles/a_root_link (rf-root-symlink)
        # 3. runfiles/my_ws/b_rf_file (rf-file)
        # 4. runfiles/my_ws/c_rf_link (rf-symlink)
        # 5. runfiles/my_ws/d_rf_empty (rf-empty)
        # 6. z/regular (regular)

        manifest_content = [
            f"regular|0|z/regular|{file1}",
            f"rf-file|0|b_rf_file|{file2}",  # -> runfiles/my_ws/b_rf_file
            f"rf-root-symlink|0|a_root_link|{file3}",  # -> runfiles/a_root_link
            f"regular|0|a/regular|{file3}",
            f"rf-empty|d_rf_empty",  # -> runfiles/my_ws/d_rf_empty
            f"rf-symlink|0|c_rf_link|{file3}",  # -> runfiles/my_ws/c_rf_link
        ]

        self.manifest_path.write_text("\n".join(manifest_content))

        self._create_zip(workspace_name="my_ws")

        with zipfile.ZipFile(self.output_zip, "r") as zf:
            self.assertEqual(
                zf.namelist(),
                [
                    "a/regular",
                    "runfiles/a_root_link",
                    "runfiles/my_ws/b_rf_file",
                    "runfiles/my_ws/c_rf_link",
                    "runfiles/my_ws/d_rf_empty",
                    "z/regular",
                ],
            )

    def is_symlink(self, zip_info):
        # Check upper 4 bits of external_attr for S_IFLNK
        # S_IFLNK is 0o120000 = 0xA000
        attr = zip_info.external_attr >> 16
        return (attr & 0xF000) == 0xA000


if __name__ == "__main__":
    unittest.main()
