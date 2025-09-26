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
import subprocess
import unittest
from pathlib import Path

from python.runfiles import runfiles


class PipInstallTest(unittest.TestCase):
    maxDiff = None

    def _remove_leading_dirs(self, paths):
        # Removes the first two directories (external/<reponame>)
        # to normalize what workspace and bzlmod produce.
        return ["/".join(v.split("/")[2:]) for v in paths]

    def test_entry_point(self):
        entry_point_path = os.environ.get("YAMLLINT_ENTRY_POINT")
        self.assertIsNotNone(entry_point_path)

        r = runfiles.Create()

        entry_point = Path(r.Rlocation(entry_point_path))
        self.assertTrue(entry_point.exists())

        proc = subprocess.run(
            [str(entry_point), "--version"],
            check=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        self.assertEqual(proc.stdout.decode("utf-8").strip(), "yamllint 1.28.0")

    def test_data(self):
        actual = os.environ.get("WHEEL_DATA_CONTENTS")
        self.assertIsNotNone(actual)
        actual = self._remove_leading_dirs(actual.split(" "))

        self.assertListEqual(
            actual,
            [
                "data/share/doc/packages/s3cmd/INSTALL.md",
                "data/share/doc/packages/s3cmd/LICENSE",
                "data/share/doc/packages/s3cmd/NEWS",
                "data/share/doc/packages/s3cmd/README.md",
                "data/share/man/man1/s3cmd.1",
            ],
        )

    def test_dist_info(self):
        actual = os.environ.get("WHEEL_DIST_INFO_CONTENTS")
        self.assertIsNotNone(actual)
        actual = self._remove_leading_dirs(actual.split(" "))
        self.assertListEqual(
            actual,
            [
                "site-packages/requests-2.25.1.dist-info/INSTALLER",
                "site-packages/requests-2.25.1.dist-info/LICENSE",
                "site-packages/requests-2.25.1.dist-info/METADATA",
                "site-packages/requests-2.25.1.dist-info/RECORD",
                "site-packages/requests-2.25.1.dist-info/WHEEL",
                "site-packages/requests-2.25.1.dist-info/top_level.txt",
            ],
        )


if __name__ == "__main__":
    unittest.main()
