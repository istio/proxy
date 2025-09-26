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

import datetime
import filecmp
import unittest
import zipfile

from python.runfiles import runfiles


# Unix dir bit and Windows dir bit. Magic from zip spec
UNIX_DIR_BIT = 0o40000
MSDOS_DIR_BIT = 0x10
UNIX_RWX_BITS = 0o777
UNIX_RX_BITS = 0o555

# The ZIP epoch date: (1980, 1, 1, 0, 0, 0)
_ZIP_EPOCH_DT = datetime.datetime(1980, 1, 1, 0, 0, 0, tzinfo=datetime.timezone.utc)
_ZIP_EPOCH_S = int(_ZIP_EPOCH_DT.timestamp())

def seconds_to_ziptime(s):
  dt = datetime.datetime.fromtimestamp(s, tz=datetime.timezone.utc)
  return (dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second)


class ZipTest(unittest.TestCase):

  def setUp(self):
    super(ZipTest, self).setUp()
    self.data_files = runfiles.Create()

  def get_test_zip(self, zip_file):
    """Get the file path to a generated zip in the runfiles."""

    return self.data_files.Rlocation("rules_pkg/tests/zip/" + zip_file)


class ZipContentsTestBase(ZipTest):
  """Use zipfile to check the contents of some generated zip files."""

  def assertZipFileContent(self, zip_file, content):
    """Assert that zip_file contains the entries described by content.

    Args:
        zip_file: the test-package-relative path to a zip file to test.
        content: an array of dictionaries containing a filename and crc key,
                 and optionally a timestamp key.
    """
    with zipfile.ZipFile(self.get_test_zip(zip_file)) as f:
      infos = f.infolist()
      self.assertEqual(len(infos), len(content))

      for info, expected in zip(infos, content):
        self.assertEqual(info.filename, expected["filename"])
        if "crc" in expected:
          self.assertEqual(info.CRC, expected["crc"])

        ts = seconds_to_ziptime(expected.get("timestamp", _ZIP_EPOCH_S))
        self.assertEqual(info.date_time, ts)
        if "isdir" in expected:
          expect_dir_bits = UNIX_DIR_BIT << 16 | MSDOS_DIR_BIT
          self.assertEqual(oct(info.external_attr & expect_dir_bits),
                           oct(expect_dir_bits))
          self.assertEqual(oct((info.external_attr >> 16) & UNIX_RWX_BITS),
                           oct(expected.get("attr", 0o755)))
        elif "isexe" in expected:
          got_mode = (info.external_attr >> 16) & UNIX_RX_BITS
          self.assertEqual(oct(got_mode), oct(UNIX_RX_BITS))
        elif "size" in expected:
          self.assertEqual(info.compress_size, expected["size"])

        else:
          if "attr" in expected:
            attr = expected.get("attr")
            if "attr_mask" in expected:
              attr &= expected.get("attr_mask")
          else:
            # I would argue this is a dumb choice, but it matches the
            # legacy rule implementation.
            attr = 0o555
          self.assertEqual(oct((info.external_attr >> 16) & UNIX_RWX_BITS),
                           oct(attr),
                           msg = info.filename)
