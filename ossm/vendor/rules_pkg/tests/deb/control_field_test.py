# Copyright 2022 The Bazel Authors. All rights reserved.
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
#
# -*- coding: utf-8 -*-
"""Testing for archive."""

import codecs
from io import BytesIO
import os
import sys
import tarfile
import unittest

from pkg.private.deb import make_deb

class MakeControlFieldTest(unittest.TestCase):
  """Tests for MakeControlField.

  https://www.debian.org/doc/debian-policy/ch-controlfields.html#syntax-of-control-files
  """

  def test_simple(self):
    self.assertEqual(
        'Package: fizzbuzz\n',
        make_deb.MakeDebianControlField('Package', 'fizzbuzz'))

  def test_simple_strip(self):
    self.assertEqual(
        'Package: fizzbuzz\n',
        make_deb.MakeDebianControlField('Package', ' fizzbuzz'))
    self.assertEqual(
        'Package: fizzbuzz\n',
        make_deb.MakeDebianControlField('Package', ' fizzbuzz '))

  def test_simple_no_newline(self):
    with self.assertRaises(ValueError):
      make_deb.MakeDebianControlField('Package', ' fizz\nbuzz ')


  def test_multiline(self):
    self.assertEqual(
        'Description: fizzbuzz\n',
        make_deb.MakeDebianControlField(
            'Description', 'fizzbuzz', multiline=make_deb.Multiline.YES))
    self.assertEqual(
        'Description: fizz\n buzz\n',
        make_deb.MakeDebianControlField(
            'Description', 'fizz\n buzz\n', multiline=make_deb.Multiline.YES))
    self.assertEqual(
        'Description:\n fizz\n buzz\n',
        make_deb.MakeDebianControlField(
            'Description', ' fizz\n buzz\n', multiline=make_deb.Multiline.YES_ADD_NEWLINE))

  def test_multiline_add_required_space(self):
    self.assertEqual(
        'Description: fizz\n buzz\n',
        make_deb.MakeDebianControlField(
            'Description', 'fizz\nbuzz', multiline=make_deb.Multiline.YES))
    self.assertEqual(
        'Description:\n fizz\n buzz\n',
        make_deb.MakeDebianControlField(
            'Description', 'fizz\nbuzz\n', multiline=make_deb.Multiline.YES_ADD_NEWLINE))

  def test_multiline_add_trailing_newline(self):
    self.assertEqual(
        'Description: fizz\n buzz\n baz\n',
        make_deb.MakeDebianControlField(
            'Description', 'fizz\n buzz\n baz', multiline=make_deb.Multiline.YES))


if __name__ == '__main__':
  unittest.main()
