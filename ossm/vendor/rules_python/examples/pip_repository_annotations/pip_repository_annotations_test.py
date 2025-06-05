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

from rules_python.python.runfiles import runfiles


class PipRepositoryAnnotationsTest(unittest.TestCase):
    maxDiff = None

    def wheel_pkg_dir(self) -> str:
        env = os.environ.get("WHEEL_PKG_DIR")
        self.assertIsNotNone(env)
        return env

    def test_build_content_and_data(self):
        r = runfiles.Create()
        rpath = r.Rlocation(
            "pip_repository_annotations_example/external/{}/generated_file.txt".format(
                self.wheel_pkg_dir()
            )
        )
        generated_file = Path(rpath)
        self.assertTrue(generated_file.exists())

        content = generated_file.read_text().rstrip()
        self.assertEqual(content, "Hello world from build content file")

    def test_copy_files(self):
        r = runfiles.Create()
        rpath = r.Rlocation(
            "pip_repository_annotations_example/external/{}/copied_content/file.txt".format(
                self.wheel_pkg_dir()
            )
        )
        copied_file = Path(rpath)
        self.assertTrue(copied_file.exists())

        content = copied_file.read_text().rstrip()
        self.assertEqual(content, "Hello world from copied file")

    def test_copy_executables(self):
        r = runfiles.Create()
        rpath = r.Rlocation(
            "pip_repository_annotations_example/external/{}/copied_content/executable{}".format(
                self.wheel_pkg_dir(),
                ".exe" if platform.system() == "windows" else ".py",
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
        current_wheel_version = "0.38.4"

        r = runfiles.Create()
        dist_info_dir = "pip_repository_annotations_example/external/{}/site-packages/wheel-{}.dist-info".format(
            self.wheel_pkg_dir(),
            current_wheel_version,
        )

        # Note: `METADATA` is important as it's consumed by https://docs.python.org/3/library/importlib.metadata.html
        # `METADATA` is expected to be there to show dist-info files are included in the runfiles.
        metadata_path = r.Rlocation("{}/METADATA".format(dist_info_dir))

        # However, `WHEEL` was explicitly excluded, so it should be missing
        wheel_path = r.Rlocation("{}/WHEEL".format(dist_info_dir))

        # Because windows does not have `--enable_runfiles` on by default, the
        # `runfiles.Rlocation` results will be different on this platform vs
        # unix platforms. See `@rules_python//python/runfiles` for more details.
        if platform.system() == "Windows":
            self.assertIsNotNone(metadata_path)
            self.assertIsNone(wheel_path)
        else:
            self.assertTrue(Path(metadata_path).exists())
            self.assertFalse(Path(wheel_path).exists())

    def requests_pkg_dir(self) -> str:
        env = os.environ.get("REQUESTS_PKG_DIR")
        self.assertIsNotNone(env)
        return env

    def test_extra(self):
        # This test verifies that annotations work correctly for pip packages with extras
        # specified, in this case requests[security].
        r = runfiles.Create()
        rpath = r.Rlocation(
            "pip_repository_annotations_example/external/{}/generated_file.txt".format(
                self.requests_pkg_dir()
            )
        )
        generated_file = Path(rpath)
        self.assertTrue(generated_file.exists())

        content = generated_file.read_text().rstrip()
        self.assertEqual(content, "Hello world from requests")


if __name__ == "__main__":
    unittest.main()
