# Copyright 2015 The Bazel Authors. All rights reserved.
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
"""Testing for archive."""

import unittest

from python.runfiles import runfiles
from pkg.private import archive


class SimpleArReaderTest(unittest.TestCase):
  """Testing for SimpleArReader class."""

  def setUp(self):
    super(SimpleArReaderTest, self).setUp()
    self.data_files = runfiles.Create()

  def assertArFileContent(self, arfile, content):
    """Assert that arfile contains exactly the entry described by `content`.

    Args:
      arfile: the path to the AR file to test.
      content: an array describing the expected content of the AR file.
          Each entry in that list should be a dictionary where each field
          is a field to test in the corresponding SimpleArFileEntry. For
          testing the presence of a file "x", then the entry could simply
          be `{"filename": "x"}`, the missing field will be ignored.
    """
    print("READING: %s" % arfile)
    with archive.SimpleArReader(arfile) as f:
      current = f.next()
      i = 0
      while current:
        error_msg = "Extraneous file at end of archive %s: %s" % (
            arfile,
            current.filename
            )
        self.assertLess(i, len(content), error_msg)
        for k, v in content[i].items():
          value = getattr(current, k)
          error_msg = " ".join([
              "Value `%s` for key `%s` of file" % (value, k),
              "%s in archive %s does" % (current.filename, arfile),
              "not match expected value `%s`" % v
              ])
          self.assertEqual(value, v, error_msg)
        current = f.next()
        i += 1
      if i < len(content):
        self.fail("Missing file %s in archive %s" % (content[i], arfile))

  def testEmptyArFile(self):
    self.assertArFileContent(
        self.data_files.Rlocation("rules_pkg/tests/testdata/empty.ar"),
        [])

  def assertSimpleFileContent(self, names):
    datafile = self.data_files.Rlocation(
        "rules_pkg/tests/testdata/" + "_".join(names) + ".ar")
    # pylint: disable=g-complex-comprehension
    content = [{"filename": n,
                "size": len(n.encode("utf-8")),
                "data": n.encode("utf-8")}
               for n in names]
    self.assertArFileContent(datafile, content)

  def testAFile(self):
    self.assertSimpleFileContent(["a"])

  def testBFile(self):
    self.assertSimpleFileContent(["b"])

  def testABFile(self):
    self.assertSimpleFileContent(["ab"])

  def testA_BFile(self):
    self.assertSimpleFileContent(["a", "b"])

  def testA_ABFile(self):
    self.assertSimpleFileContent(["a", "ab"])

  def testA_B_ABFile(self):
    self.assertSimpleFileContent(["a", "b", "ab"])


if __name__ == "__main__":
  unittest.main()
