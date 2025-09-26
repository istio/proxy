#!/usr/bin/env python3
# Copyright 2023 The Bazel Authors. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.


import os
import platform
import subprocess
import sys
import unittest
from pathlib import Path

from python.runfiles import runfiles


class PipWhlModsTest(unittest.TestCase):
    maxDiff = None

    @staticmethod
    def _get_bazel_pkg_dir_name(env_var: str) -> str:
        a_file = Path(os.environ.get(env_var).split(" ")[0])
        head = a_file
        while head.parent.name:
            head = head.parent

        return head.name

    @classmethod
    def setUpClass(cls):
        cls._wheel_pkg_dir = cls._get_bazel_pkg_dir_name("WHEEL_PKG")
        cls._requests_pkg_dir = cls._get_bazel_pkg_dir_name("REQUESTS_PKG")

    def wheel_pkg_dir(self) -> Path:
        return self._wheel_pkg

    def test_build_content_and_data(self):
        r = runfiles.Create()
        rpath = r.Rlocation(
            "{}/generated_file.txt".format(
                self._wheel_pkg_dir,
            ),
        )
        generated_file = Path(rpath)
        self.assertTrue(generated_file.exists())

        content = generated_file.read_text().rstrip()
        self.assertEqual(content, "Hello world from build content file")

    def test_copy_files(self):
        r = runfiles.Create()
        rpath = r.Rlocation(
            "{}/copied_content/file.txt".format(
                self._wheel_pkg_dir,
            )
        )
        copied_file = Path(rpath)
        self.assertTrue(copied_file.exists())

        content = copied_file.read_text().rstrip()
        self.assertEqual(content, "Hello world from copied file")

    def test_copy_executables(self):
        executable_name = (
            "executable.exe" if platform.system() == "windows" else "executable.py"
        )

        r = runfiles.Create()
        rpath = r.Rlocation(
            "{}/copied_content/{}".format(
                self._wheel_pkg_dir,
                executable_name,
            )
        )
        executable = Path(rpath)
        self.assertTrue(executable.exists())

        proc = subprocess.run(
            [sys.executable, str(executable)],
            check=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        stdout = proc.stdout.decode("utf-8").strip()
        self.assertEqual(stdout, "Hello world from copied executable")

    def test_data_exclude_glob(self):
        current_wheel_version = "0.40.0"

        r = runfiles.Create()
        dist_info_dir = "{}/site-packages/wheel-{}.dist-info".format(
            self._wheel_pkg_dir,
            current_wheel_version,
        )

        # Note: `METADATA` is important as it's consumed by https://docs.python.org/3/library/importlib.metadata.html
        # `METADATA` is expected to be there to show dist-info files are included in the runfiles.
        metadata_path = r.Rlocation("{}/METADATA".format(dist_info_dir))

        # However, `WHEEL` was explicitly excluded, so it should be missing
        wheel_path = r.Rlocation("{}/WHEEL".format(dist_info_dir))

        self.assertTrue(Path(metadata_path).exists(), f"Could not find {metadata_path}")
        self.assertFalse(
            Path(wheel_path).exists(), f"Expected to not find {wheel_path}"
        )

    def test_extra(self):
        # This test verifies that annotations work correctly for pip packages with extras
        # specified, in this case requests[security].
        r = runfiles.Create()
        rpath = r.Rlocation(
            "{}/generated_file.txt".format(
                self._requests_pkg_dir,
            ),
        )
        generated_file = Path(rpath)
        self.assertTrue(generated_file.exists())

        content = generated_file.read_text().rstrip()
        self.assertEqual(content, "Hello world from requests")

    def test_patches(self):
        current_wheel_version = "2.25.1"

        # This test verifies that the patches are applied to the wheel.
        r = runfiles.Create()
        metadata_path = "{}/site-packages/requests-{}.dist-info/METADATA".format(
            self._requests_pkg_dir,
            current_wheel_version,
        )

        metadata = Path(r.Rlocation(metadata_path))
        self.assertIn(
            "Summary: Python HTTP for Humans. Patched.",
            metadata.read_text().splitlines(),
        )


if __name__ == "__main__":
    unittest.main()
