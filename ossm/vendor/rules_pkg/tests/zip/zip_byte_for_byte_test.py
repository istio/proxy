# Copyright 2019 The Bazel Authors. All rights reserved.
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

import filecmp
import unittest

from tests.zip import zip_test_lib


class ZipEquivalency(zip_test_lib.ZipTest):
  """Check that some generated zip files are equivalent to each-other."""

  def assertFilesEqual(self, actual, expected):
    """Assert that two zip files contain the same bytes."""

    zips_are_equal = filecmp.cmp(
        self.get_test_zip(actual),
        self.get_test_zip(expected),
    )
    self.assertTrue(zips_are_equal)

  def test_small_timestamp(self):
    self.assertFilesEqual(
        "test_zip_basic_timestamp_before_epoch.zip",
        "test_zip_basic.zip",
    )

  def test_extension(self):
    self.assertFilesEqual(
        "test_zip_basic_renamed.foo",
        "test_zip_basic.zip",
    )

  def test_package_dir1(self):
    self.assertFilesEqual(
        "test_zip_package_dir1.zip",
        "test_zip_package_dir0.zip",
    )

  def test_package_dir2(self):
    self.assertFilesEqual(
        "test_zip_package_dir2.zip",
        "test_zip_package_dir0.zip",
    )

if __name__ == "__main__":
  unittest.main()
