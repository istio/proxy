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

import os
import tempfile
import unittest

from pkg.private import helpers


class GetFlagValueTestCase(unittest.TestCase):

  def testNonStripped(self):
    self.assertEqual(helpers.GetFlagValue('value ', strip=False), 'value ')

  def testStripped(self):
    self.assertEqual(helpers.GetFlagValue('value ', strip=True), 'value')

  def testNonStripped_fromFile(self):
    with tempfile.TemporaryDirectory() as temp_d:
      argfile_path = os.path.join(temp_d, 'argfile')
      with open(argfile_path, 'wb') as f:
        f.write(b'value ')
      self.assertEqual(
          helpers.GetFlagValue('@'+argfile_path, strip=False), 'value ')

  def testStripped_fromFile(self):
    with tempfile.TemporaryDirectory() as temp_d:
      argfile_path = os.path.join(temp_d, 'argfile')
      with open(argfile_path, 'wb') as f:
        f.write(b'value ')
      self.assertEqual(
          helpers.GetFlagValue('@'+argfile_path, strip=True), 'value')


class SplitNameValuePairAtSeparatorTestCase(unittest.TestCase):

  def testNoSep(self):
    key, val = helpers.SplitNameValuePairAtSeparator('abc', '=')
    self.assertEqual(key, 'abc')
    self.assertEqual(val, '')

  def testNoSepWithEscape(self):
    key, val = helpers.SplitNameValuePairAtSeparator('a\\=bc', '=')
    self.assertEqual(key, 'a=bc')
    self.assertEqual(val, '')

  def testNoSepWithDanglingEscape(self):
    key, val = helpers.SplitNameValuePairAtSeparator('abc\\', '=')
    self.assertEqual(key, 'abc')
    self.assertEqual(val, '')

  def testHappyCase(self):
    key, val = helpers.SplitNameValuePairAtSeparator('abc=xyz', '=')
    self.assertEqual(key, 'abc')
    self.assertEqual(val, 'xyz')

  def testHappyCaseWithEscapes(self):
    key, val = helpers.SplitNameValuePairAtSeparator('a\\=\\=b\\=c=xyz', '=')
    self.assertEqual(key, 'a==b=c')
    self.assertEqual(val, 'xyz')

  def testStopsAtFirstSep(self):
    key, val = helpers.SplitNameValuePairAtSeparator('a=b=c', '=')
    self.assertEqual(key, 'a')
    self.assertEqual(val, 'b=c')

  def testDoesntUnescapeVal(self):
    key, val = helpers.SplitNameValuePairAtSeparator('abc=x\\=yz\\', '=')
    self.assertEqual(key, 'abc')
    # the val doesn't get unescaped at all
    self.assertEqual(val, 'x\\=yz\\')

  def testUnescapesNonsepCharsToo(self):
    key, val = helpers.SplitNameValuePairAtSeparator('na\\xffme=value', '=')
    # this behaviour is surprising
    self.assertEqual(key, 'naxffme')
    self.assertEqual(val, 'value')

if __name__ == '__main__':
  unittest.main()
