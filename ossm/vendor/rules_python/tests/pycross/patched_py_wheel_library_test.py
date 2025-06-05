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

import unittest
from pathlib import Path

from python.runfiles import runfiles

RUNFILES = runfiles.Create()


class TestPyWheelLibrary(unittest.TestCase):
    def setUp(self):
        self.extraction_dir = Path(
            RUNFILES.Rlocation(
                "rules_python/tests/pycross/patched_extracted_wheel_for_testing"
            )
        )
        self.assertTrue(self.extraction_dir.exists(), self.extraction_dir)
        self.assertTrue(self.extraction_dir.is_dir(), self.extraction_dir)

    def test_patched_file_contents(self):
        """Validate that the patch got applied correctly."""
        file = self.extraction_dir / "site-packages/numpy/file_added_via_patch.txt"
        self.assertEqual(file.read_text(), "Hello from a patch!\n")


if __name__ == "__main__":
    unittest.main()
