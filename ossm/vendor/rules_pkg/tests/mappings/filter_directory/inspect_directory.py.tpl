#!/usr/bin/env python3

# Copyright 2021 The Bazel Authors. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import json
import os
import unittest
from python.runfiles import runfiles

DIRECTORY_ROOT = "%DIRECTORY_ROOT%"
# This is JSON, which shouldn't have any triple quotes in it.
EXPECTED_STRUCTURE = """%EXPECTED_STRUCTURE%"""


class DirectoryStructureTest(unittest.TestCase):
    def setUp(self):
        self.runfiles = runfiles.Create()

    def test_directory_structure_matches_global(self):
        real_directory_root = self.runfiles.Rlocation(
            os.path.join(os.environ["TEST_WORKSPACE"], DIRECTORY_ROOT)
        )

        # This may be a bazel bug -- shouldn't an empty directory be passed in
        # anyway?
        self.assertTrue(
            os.path.isdir(real_directory_root),
            "TreeArtifact root does not exist, is the input empty?",
        )

        expected_set = set(json.loads(EXPECTED_STRUCTURE))
        actual_set = set()
        for root, dirs, files in os.walk(real_directory_root):
            if root != real_directory_root:
                rel_root = os.path.relpath(root, real_directory_root)
            else:
                # We are in the root.  Don't bother with path relativization.
                rel_root = ''
            for f in files:
                actual_set.add(os.path.join(rel_root, f))

        self.assertEqual(
            expected_set,
            actual_set,
            "Directory structure mismatch"
        )


if __name__ == "__main__":
    unittest.main()
