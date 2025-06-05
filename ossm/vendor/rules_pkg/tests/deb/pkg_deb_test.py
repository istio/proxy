# Copyright 2020 The Bazel Authors. All rights reserved.
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

from python.runfiles import runfiles
from pkg.private import archive


class DebInspect(object):
  """Class to open and unpack a .deb file so we can examine it."""

  def __init__(self, deb_file):
    self.deb_version = None
    self.data = None
    self.control = None
    with archive.SimpleArReader(deb_file) as f:
      info = f.next()
      while info:
        if info.filename == 'debian-binary':
          self.deb_version = info.data
        elif info.filename == 'control.tar.gz':
          self.control = info.data
        elif info.filename == 'data.tar.gz':
          self.data = info.data
        else:
          raise Exception('Unexpected file: %s' % info.filename)
        info = f.next()

  def get_deb_ctl_file(self, file_name):
    """Extract a control file."""

    with tarfile.open(mode='r:gz', fileobj=BytesIO(self.control)) as f:
      for info in f:
        if info.name == './' + file_name:
          return codecs.decode(f.extractfile(info).read(), 'utf-8')
    raise Exception('Could not find control file: %s' % file_name)


class PkgDebTest(unittest.TestCase):
  """Testing for pkg_deb rule."""

  def setUp(self):
    super(PkgDebTest, self).setUp()
    self.runfiles = runfiles.Create()
    # Note: Rlocation requires forward slashes. os.path.join() will not work.
    self.deb_path = self.runfiles.Rlocation('rules_pkg/tests/deb/fizzbuzz_4.5.6_all.deb')
    self.deb_file = DebInspect(self.deb_path)

  def assert_control_content(self, expected, match_order=False):
    self.assert_tar_stream_content(
        BytesIO(self.deb_file.control),
        expected,
        match_order=match_order)

  def assert_data_content(self, expected, match_order=True):
    self.assert_tar_stream_content(
        BytesIO(self.deb_file.data),
        expected,
        match_order=match_order)

  def assert_tar_stream_content(self, data, expected, match_order=True):
    """Assert that tarfile contains exactly the entry described by `expected`.

    Args:
        data: the tar file stream to check.
        expected: an array describing the expected content of the TAR file.
            Each entry in that list should be a dictionary where each field
            is a field to test in the corresponding TarInfo. For
            testing the presence of a file "x", then the entry could simply
            be `{'name': 'x'}`, the missing field will be ignored. To match
            the content of a file entry, use the key 'data'.
        match_order: True if files must match in order as well as properties.
    """
    expected_by_name = {}
    for e in expected:
      expected_by_name[e['name']] = e

    with tarfile.open(mode='r:*', fileobj=data) as f:
      i = 0
      for info in f:
        error_msg = 'Extraneous file at end of archive: %s' % (
            info.name
            )
        self.assertLess(i, len(expected), error_msg)
        name_in_tar_file = getattr(info, 'name')
        if match_order:
          want = expected[i]
        else:
          want = expected_by_name[name_in_tar_file]
        for k, v in want.items():
          if k == 'data':
            value = f.extractfile(info).read()
          elif k == 'name':
            # The test data uses / as path sep, but the tarball is in OS native
            # format. This aligns the tarball name back to what we expect.
            value = name_in_tar_file.replace(os.path.sep, '/')
          elif k == 'isdir':
            value = info.isdir()
          elif k == "name" and sys.platform == 'win32':
            value = getattr(current, k).replace("\\", "/")
          else:
            value = getattr(info, k)
          error_msg = ' '.join([
              'Value `%s` for key `%s` of file' % (value, k),
              '%s in archive does' % info.name,
              'not match expected value `%s`' % v
              ])
          self.assertEqual(value, v, error_msg)
        i += 1
      if i < len(expected):
        self.fail('Missing file %s' % expected[i])

  def test_expected_files(self):
    # Check the set of 'test-tar-basic-*' smoke test.
    expected = [
        {'name': 'etc', 'isdir': True,
         'uid': 24, 'gid': 42, 'uname': 'foobar', 'gname': 'fizzbuzz'},
        {'name': 'etc/nsswitch.conf',
         'mode': 0o644,
         'uid': 24, 'gid': 42, 'uname': 'foobar', 'gname': 'fizzbuzz'
         },
        {'name': 'usr', 'isdir': True,
         'uid': 42, 'gid': 24, 'uname': 'fizzbuzz', 'gname': 'foobar'},
        {'name': 'usr/bin', 'isdir': True},
        {'name': 'usr/bin/java', 'linkname': '/path/to/bin/java'},
        {'name': 'usr/fizzbuzz',
         'mode': 0o755,
         'uid': 42, 'gid': 24, 'uname': 'fizzbuzz', 'gname': 'foobar'},
    ]
    self.assert_data_content(expected)

  def test_description(self):
    control = self.deb_file.get_deb_ctl_file('control')
    fields = [
        'Package: fizzbuzz',
        'Depends: dep1, dep2',
        'Built-Using: some_test_data',
        'Replaces: oldpkg',
        'Breaks: oldbrokenpkg',
        'Provides: hello',
        'License: Apache-2.0',
    ]
    # TODO(https://github.com/bazelbuild/rules_pkg/issues/214): This can not
    # pass on Windows Until we rewrite how description is passed
    if sys.platform != 'win32':
      fields.extend([
          'Description: toto ®, Й, ק ,م, ๗, あ, 叶, 葉, 말, ü and é\n more\n',
          'Maintainer: soméone@somewhere.com',
      ])
    for field in fields:
      if control.find(field) < 0:
        self.fail('Missing control field: <%s> in <%s>' % (field, control))

  def test_control_files(self):
    expected = [
        {'name': './conffiles', 'mode': 0o644},
        {'name': './config', 'mode': 0o755},
        {'name': './control', 'mode': 0o644},
        {'name': './preinst', 'mode': 0o755},
        {'name': './templates', 'mode': 0o644},
        {'name': './triggers', 'mode': 0o644},
    ]
    self.assert_control_content(expected, match_order=False)

  def test_conffiles(self):
    conffiles = self.deb_file.get_deb_ctl_file('conffiles')
    self.assertEqual(
        conffiles,
        '/etc/nsswitch.conf\n/etc/other\n')

  def test_config(self):
    config = self.deb_file.get_deb_ctl_file('config')
    self.assertEqual(config, '# test config file\n')

  def test_preinst(self):
    preinst = self.deb_file.get_deb_ctl_file('preinst')
    self.assertEqual(
        preinst,
        '#!/usr/bin/env bash\n'
        '# tete ®, Й, ק ,م, ๗, あ, 叶, 葉, 말, ü and é\n'
        'echo fnord\n')

  def test_templates(self):
    templates = self.deb_file.get_deb_ctl_file('templates')
    for field in ('Template: deb/test', 'Type: string'):
      if templates.find(field) < 0:
        self.fail('Missing template field: <%s> in <%s>' % (field, templates))

  def test_triggers(self):
    triggers = self.deb_file.get_deb_ctl_file('triggers')
    self.assertEqual(
        triggers,
        '# tutu ®, Й, ק ,م, ๗, あ, 叶, 葉, 말, ü and é\n'
        'some-trigger\n')

  def test_changes(self):
    changes_path = self.runfiles.Rlocation(
        'rules_pkg/tests/deb/fizzbuzz_4.5.6_all.changes')
    with open(changes_path, 'r', encoding='utf-8') as f:
      content = f.read()
      for field in (
          'Urgency: low',
          'Distribution: trusty'):
        if content.find(field) < 0:
          self.fail('Missing template field: <%s> in <%s>' % (field, content))

      # TODO(https://github.com/bazelbuild/rules_pkg/issues/214): This can not
      # pass on Windows until we rewrite how description is passed.
      if sys.platform == 'win32':
        return

      # From the spec:
      #   In a .changes file, the Description field contains a summary of the
      #   descriptions for the packages being uploaded. For this case, the first
      #   line of the field value (the part on the same line as Description:) is
      #   always empty. It is a multiline field, with one line per package. Each
      #   line is indented by one space and contains the name of a binary package,
      #   a space, a hyphen (-), a space, and the short description line from that
      #   package.
      d_expect = 'Description:\n fizzbuzz - toto ®, Й, ק ,م, ๗, あ, 叶, 葉, 말, ü and é\n'
      d_start = content.find(d_expect)
      if d_start < 0:
        self.fail(
            'Could not find expected description (%s) in\n=====%s=====' %
            (d_expect, content))
      # Check that the next line is the start of a new description, rather than
      # a continuation.
      self.assertTrue(
          content[d_start + len(d_expect)].isupper(),
          'Description has unexpected characters at end (%s)' % content)

      self.maxDiff = None
      expect = '''Format: 1\.8
Date: Thu Jan  1 \d{2}:00:00 1970
Source: fizzbuzz
Binary: fizzbuzz
Architecture: all
Version: 4\.5\.6
Distribution: trusty
Urgency: low
Maintainer: soméone@somewhere.com
Changed-By: soméone@somewhere.com
Description:
 fizzbuzz - toto ®, Й, ק ,م, ๗, あ, 叶, 葉, 말, ü and é
Changes:
 fizzbuzz \(4\.5\.6\) trusty; urgency=low
 Changes are tracked in revision control\.
Files:
 [a-f0-9]{32} \d{4} contrib/devel optional fizzbuzz_4\.5\.6_all\.deb
Checksums-Sha1:
 [a-f0-9]{40} \d{4} fizzbuzz_4\.5\.6_all\.deb
Checksums-Sha256:
 [a-f0-9]{64} \d{4} fizzbuzz_4\.5\.6_all\.deb
'''

      self.assertRegex(content, expect)


if __name__ == '__main__':
  unittest.main()
