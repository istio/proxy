# Copyright 2023 The Bazel Authors. All rights reserved.
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
"""Tests for generated content manifest."""

import re
import tarfile
import unittest

class VerifyArchiveTest(unittest.TestCase):
  """Test harness to see if we wrote the content manifest correctly."""


  def setUp(self):
    super(VerifyArchiveTest, self).setUp()

  def scan_target(self, target):
    parts = target.split('.')
    ext = parts[-1]
    if ext[0] == 't' or parts[-2] == 'tar':
      self.load_tar(target)
    elif ext[0] == 'z':
      self.fail('Can not process zip yet')
    else:
      self.fail('Can not figure out the archive type for (%s)' % target)

  def load_tar(self, path):
    self.paths = []
    self.links = {}
    with tarfile.open(path, 'r:*') as f:
      i = 0
      for info in f:
        self.paths.append(info.name)
        if info.linkname:
          self.links[info.name] = info.linkname

  def assertMinSize(self, min_size):
    """Check that the archive contains at least min_size entries.

    Args:
        min_size: The minimum number of targets we expect.
    """
    if min_size <= 0:
      return
    actual_size = len(self.paths)
    self.assertGreaterEqual(
        len(self.paths),
        min_size,
        msg = "Expected at least %d files, but found only %d" % (
            min_size, actual_size))

  def assertMaxSize(self, max_size):
    """Check that the archive contains at most max_size entries.

    Args:
        max_size: The maximum number of targets we expect.
    """
    if max_size <= 0:
      return
    actual_size = len(self.paths)
    self.assertLessEqual(
        len(self.paths),
        max_size,
        msg = "Expected at most %d files, but found %d" % (
            max_size, actual_size))

  def check_must_contain(self, must_contain):
    plain_patterns = set(must_contain)
    for path in self.paths:
      if path in plain_patterns:
        plain_patterns.remove(path)
    if len(plain_patterns) > 0:
      self.fail('These required paths were not found: %s' % ','.join(plain_patterns) + ' in [%s]' % ','.join(self.paths))

  def check_must_not_contain(self, must_not_contain):
    plain_patterns = set(must_not_contain)
    for path in self.paths:
      if path in plain_patterns:
        self.fail('Found disallowed path (%s) in the archive' % path)

  def check_must_contain_regex(self, must_contain_regex):
    for pattern in must_contain_regex:
      r_comp = re.compile(pattern)
      matched = False
      for path in self.paths:
        if r_comp.match(path):
          matched = True
          break
      if not matched:
        self.fail('Did not find pattern (%s) in the archive' % pattern)

  def check_must_not_contain_regex(self, must_not_contain_regex):
    for pattern in must_not_contain_regex:
      r_comp = re.compile(pattern)
      for path in self.paths:
        if r_comp.match(path):
          self.fail('Found disallowed pattern (%s) in the archive' % pattern)

  def verify_links(self, verify_links):
    for link, target in verify_links.items():
      if link not in self.paths:
        self.fail('Required link (%s) is not in the archive, found %s' % (link, self.paths))
      if self.links[link] != target:
        self.fail('link (%s) points to the wrong place. Expected (%s) got (%s)' %
            (link, target, self.links[link]))


class ${TEST_NAME}(VerifyArchiveTest):

  def setUp(self):
    super(${TEST_NAME}, self).setUp()
    self.scan_target('${TARGET}')

  def test_min_size(self):
    self.assertMinSize(${MIN_SIZE})

  def test_max_size(self):
    self.assertMaxSize(${MAX_SIZE})

  def test_must_contain(self):
    self.check_must_contain(${MUST_CONTAIN})

  def test_must_not_contain(self):
    self.check_must_not_contain(${MUST_NOT_CONTAIN})

  def test_must_contain_regex(self):
    self.check_must_contain_regex(${MUST_CONTAIN_REGEX})

  def test_must_not_contain_regex(self):
    self.check_must_not_contain_regex(${MUST_NOT_CONTAIN_REGEX})

  def test_verify_links(self):
    self.verify_links(${VERIFY_LINKS})


if __name__ == '__main__':
  unittest.main()
