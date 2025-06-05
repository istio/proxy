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
"""Tests if input files are compressed."""

import unittest

from python.runfiles import runfiles


class IsCompressedTest(unittest.TestCase):

  def setUp(self):
    self.data_files = runfiles.Create()

  def get_file_under_test(self, file_name):
    """Get the file path to a generated archive in the runfiles."""

    return self.data_files.Rlocation(
        "rules_pkg/tests/tar/" + file_name
    )

  def is_zip_compressed(self, file_name):
    """Returns true if file_name is zip compressed."""
    with open(self.get_file_under_test(file_name), 'rb') as inp:
      content = inp.read(2)
      # A quick web search will show these magic constants are correct.
      return content[0] == 0x1f and content[1] == 0x8b

  def is_bz2_compressed(self, file_name):
    """Returns true if file_name is bz2 compressed."""
    with open(self.get_file_under_test(file_name), 'rb') as inp:
      content = inp.read(7)
      # A quick web search will show these magic constants are correct.
      # This is probably well beyond what we need in this test, but why not?
      return (content[0] == ord('B') and content[1] == ord('Z')
              and ord('1') <= content[3] and content[3] <= ord('9')
              and content[4] == 0x31 and content[5] == 0x41 and content[6] == 0x59)

  def test_guess_compression_from_extension(self):
    self.assertTrue(
        self.is_bz2_compressed("test-tar-compression-from-extension-bz2.bz2"))
    self.assertTrue(
        self.is_zip_compressed("test-tar-compression-from-extension-targz.tar.gz"))
    self.assertTrue(
        self.is_zip_compressed("test-tar-compression-from-extension-tgz.tgz"))


if __name__ == '__main__':
  unittest.main()
