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
"""Test time stamping in pkg_tar"""

import datetime
import tarfile
import time
import unittest
import zipfile

from python.runfiles import runfiles

# keep in sync with archive.py
PORTABLE_MTIME = 946684800  # 2000-01-01 00:00:00.000 UTC


class StampTest(unittest.TestCase):
  """Test for non-epoch time stamps in packages."""

  target_mtime = int(time.time())
  zip_epoch_dt = datetime.datetime(1980, 1, 1, 0, 0, 0, tzinfo=datetime.timezone.utc)
  ZIP_EPOCH = int(zip_epoch_dt.timestamp())
  # We allow the stamp on the build file to be within this delta back in
  # time from now. That allows for the test data to be built early in a clean
  # CI run and have the test run a few seconds later. Ideally, this would
  # be no greater than the expected total CI run time, but the extra margin
  # does not hurt.
  ALLOWED_DELTA_FROM_NOW = 30000  # seconds

  def check_mtime(self, mtime, file_path, file_name):
    """Checks that a time stamp is reasonable.

    This checks that a timestamp is not 0 or any of the well known EPOCH values,
    and that it is within some delta from the current time.

    Args:
      mtime: timestamp in seconds
      file_path: path to archive name
      file_name: file within archive
    """
    if mtime == 0:
       self.fail('Archive %s contains file %s with mtime == 0' % (
           file_path, file_name))
    if mtime == PORTABLE_MTIME:
       self.fail('Archive %s contains file %s with portable mtime' % (
           file_path, file_name))
    if mtime == StampTest.ZIP_EPOCH:
       self.fail('Archive %s contains file %s with ZIP epoch' % (
           file_path, file_name))
    if ((mtime < self.target_mtime - StampTest.ALLOWED_DELTA_FROM_NOW)
        or (mtime > self.target_mtime)):
       self.fail(
           'Archive %s contains file %s with mtime:%d, expected:%d +/- %d.' % (
               file_path, file_name, mtime, self.target_mtime,
               StampTest.ALLOWED_DELTA_FROM_NOW) +
           '  This may be a false positive if your build cache is more than' +
           ' %s seconds old.  If so, try bazel clean and rerun the test.' %
           StampTest.ALLOWED_DELTA_FROM_NOW)

  def assertTarFilesAreAlmostNew(self, file_name):
    """Assert that tarfile contains files with an mtime of roughly now.

    This is used to prove that the test data was a file which was presumably:
    built with 'stamp=1' or ('stamp=-1' and --stamp) contains files which
    all have a fairly recent mtime, thus indicating they are "current" time
    rather than the epoch or some other time.

    Args:
        file_name: the path to the TAR file to test.
    """
    file_path = runfiles.Create().Rlocation('rules_pkg/tests/' + file_name)
    with tarfile.open(file_path, 'r:*') as f:
      for info in f:
        self.check_mtime(info.mtime, file_path, info.name)

  def assertZipFilesAreAlmostNew(self, file_name):
    """Assert that zipfile contains files with an mtime of roughly now.

    This is used to prove that the test data was a file which was presumably:
    built with 'stamp=1' or ('stamp=-1' and --stamp) contains files which
    all have a fairly recent mtime, thus indicating they are "current" time
    rather than the epoch or some other time.

    Args:
        file_name: the path to the ZIP file to test.
    """
    file_path = runfiles.Create().Rlocation('rules_pkg/tests/' + file_name)
    target_mtime = int(time.time())
    with zipfile.ZipFile(file_path, mode='r') as f:
      for info in f.infolist():
        d = info.date_time
        dt = datetime.datetime(d[0], d[1], d[2], d[3], d[4], d[5], tzinfo=datetime.timezone.utc)
        self.check_mtime(int(dt.timestamp()), file_path, info.filename)

  def test_not_epoch_times_tar(self):
    self.assertTarFilesAreAlmostNew('stamped_tar.tar')

  def test_not_epoch_times_zip(self):
    self.assertZipFilesAreAlmostNew('stamped_zip.zip')


if __name__ == '__main__':
  unittest.main()
