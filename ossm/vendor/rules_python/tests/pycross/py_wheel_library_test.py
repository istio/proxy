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
            RUNFILES.Rlocation("rules_python/tests/pycross/extracted_wheel_for_testing")
        )
        self.assertTrue(self.extraction_dir.exists(), self.extraction_dir)
        self.assertTrue(self.extraction_dir.is_dir(), self.extraction_dir)

    def test_file_presence(self):
        """Validate that the basic file layout looks good."""
        for path in (
            "bin/f2py",
            "site-packages/numpy.libs/libgfortran-daac5196.so.5.0.0",
            "site-packages/numpy/dtypes.py",
            "site-packages/numpy/core/_umath_tests.cpython-311-aarch64-linux-gnu.so",
        ):
            print(self.extraction_dir / path)
            self.assertTrue(
                (self.extraction_dir / path).exists(), f"{path} does not exist"
            )


if __name__ == "__main__":
    unittest.main()
