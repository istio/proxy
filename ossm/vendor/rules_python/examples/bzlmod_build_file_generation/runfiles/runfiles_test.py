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
import unittest

from other_module.pkg import lib

from python.runfiles import runfiles


class RunfilesTest(unittest.TestCase):
    # """Unit tests for `runfiles.Runfiles`."""
    def testCurrentRepository(self):
        self.assertEqual(runfiles.Create().CurrentRepository(), "")

    def testRunfilesWithRepoMapping(self):
        data_path = runfiles.Create().Rlocation(
            "example_bzlmod_build_file_generation/runfiles/data/data.txt"
        )
        with open(data_path, "rt", encoding="utf-8", newline="\n") as f:
            self.assertEqual(f.read().strip(), "Hello, example_bzlmod!")

    def testRunfileWithRlocationpath(self):
        data_rlocationpath = os.getenv("DATA_RLOCATIONPATH")
        data_path = runfiles.Create().Rlocation(data_rlocationpath)
        with open(data_path, "rt", encoding="utf-8", newline="\n") as f:
            self.assertEqual(f.read().strip(), "Hello, example_bzlmod!")

    def testRunfileInOtherModuleWithOurRepoMapping(self):
        data_path = runfiles.Create().Rlocation(
            "our_other_module/other_module/pkg/data/data.txt"
        )
        with open(data_path, "rt", encoding="utf-8", newline="\n") as f:
            self.assertEqual(f.read().strip(), "Hello, other_module!")

    def testRunfileInOtherModuleWithItsRepoMapping(self):
        data_path = lib.GetRunfilePathWithRepoMapping()
        with open(data_path, "rt", encoding="utf-8", newline="\n") as f:
            self.assertEqual(f.read().strip(), "Hello, other_module!")

    def testRunfileInOtherModuleWithCurrentRepository(self):
        data_path = lib.GetRunfilePathWithCurrentRepository()
        with open(data_path, "rt", encoding="utf-8", newline="\n") as f:
            self.assertEqual(f.read().strip(), "Hello, other_module!")

    def testRunfileInOtherModuleWithRlocationpath(self):
        data_rlocationpath = os.getenv("OTHER_MODULE_DATA_RLOCATIONPATH")
        data_path = runfiles.Create().Rlocation(data_rlocationpath)
        with open(data_path, "rt", encoding="utf-8", newline="\n") as f:
            self.assertEqual(f.read().strip(), "Hello, other_module!")


if __name__ == "__main__":
    unittest.main()
