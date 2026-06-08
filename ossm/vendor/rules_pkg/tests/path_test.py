# Copyright 2016 The Bazel Authors. All rights reserved.
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
"""Testing for helper functions."""

import collections
import imp
import os
import unittest


pkg_bzl = imp.load_source('pkg_bzl', 'pkg/path.bzl')

Owner = collections.namedtuple('Owner', ['workspace_name', 'workspace_root'])
Root = collections.namedtuple('Root', ['path'])


class File(object):
  """Mock Skylark File class for testing."""

  def __init__(self, short_path, is_generated=False, is_external=False):
    self.is_source = not is_generated
    self.root = Root('bazel-out/k8-fastbuild/bin' if is_generated else '')
    if is_external:
      self.owner = Owner('repo', 'external/repo')
      self.short_path = '../repo/' + short_path
    else:
      self.owner = Owner('', '')
      self.short_path = short_path
    self.path = os.path.join(
        self.root.path, self.owner.workspace_root, short_path)


class SafeShortPathTest(unittest.TestCase):
  """Testing for safe_short_path."""

  def testSafeShortPath(self):
    path = pkg_bzl.safe_short_path(File('foo/bar/baz'))
    self.assertEqual('foo/bar/baz', path)

  def testSafeShortPathGen(self):
    path = pkg_bzl.safe_short_path(File('foo/bar/baz', is_generated=True))
    self.assertEqual('foo/bar/baz', path)

  def testSafeShortPathExt(self):
    path = pkg_bzl.safe_short_path(File('foo/bar/baz', is_external=True))
    self.assertEqual('external/repo/foo/bar/baz', path)

  def testSafeShortPathGenExt(self):
    path = pkg_bzl.safe_short_path(
        File('foo/bar/baz', is_generated=True, is_external=True))
    self.assertEqual('external/repo/foo/bar/baz', path)


class ShortPathDirnameTest(unittest.TestCase):
  """Testing for _short_path_dirname."""

  def testShortPathDirname(self):
    path = pkg_bzl._short_path_dirname(File('foo/bar/baz'))
    self.assertEqual('foo/bar', path)

  def testShortPathDirnameGen(self):
    path = pkg_bzl._short_path_dirname(File('foo/bar/baz', is_generated=True))
    self.assertEqual('foo/bar', path)

  def testShortPathDirnameExt(self):
    path = pkg_bzl._short_path_dirname(File('foo/bar/baz', is_external=True))
    self.assertEqual('external/repo/foo/bar', path)

  def testShortPathDirnameGenExt(self):
    path = pkg_bzl._short_path_dirname(
        File('foo/bar/baz', is_generated=True, is_external=True))
    self.assertEqual('external/repo/foo/bar', path)

  def testTopLevel(self):
    path = pkg_bzl._short_path_dirname(File('baz'))
    self.assertEqual('', path)

  def testTopLevelGen(self):
    path = pkg_bzl._short_path_dirname(File('baz', is_generated=True))
    self.assertEqual('', path)

  def testTopLevelExt(self):
    path = pkg_bzl._short_path_dirname(File('baz', is_external=True))
    self.assertEqual('external/repo', path)


class DestPathTest(unittest.TestCase):
  """Testing for _dest_path."""

  def testDestPath(self):
    path = pkg_bzl.dest_path(File('foo/bar/baz'), 'foo')
    self.assertEqual('/bar/baz', path)

  def testDestPathGen(self):
    path = pkg_bzl.dest_path(File('foo/bar/baz', is_generated=True), 'foo')
    self.assertEqual('/bar/baz', path)

  def testDestPathExt(self):
    path = pkg_bzl.dest_path(
        File('foo/bar/baz', is_external=True), 'external/repo/foo')
    self.assertEqual('/bar/baz', path)

  def testDestPathExtWrong(self):
    path = pkg_bzl.dest_path(File('foo/bar/baz', is_external=True), 'foo')
    self.assertEqual('external/repo/foo/bar/baz', path)

  def testNoMatch(self):
    path = pkg_bzl.dest_path(File('foo/bar/baz'), 'qux')
    self.assertEqual('foo/bar/baz', path)

  def testNoStrip(self):
    path = pkg_bzl.dest_path(File('foo/bar/baz'), None)
    self.assertEqual('/baz', path)

  def testTopLevel(self):
    path = pkg_bzl.dest_path(File('baz'), None)
    self.assertEqual('baz', path)

  def testPartialDirectoryMatch(self):
    path = pkg_bzl.dest_path(File('foo/bar/baz'), 'fo')
    self.assertEqual('foo/bar/baz', path)

  def testPartialDirectoryMatchWithDataPath(self):
    path = pkg_bzl.dest_path(File('foo/bar/baz'), 'foo/ba', 'foo')
    self.assertEqual('/bar/baz', path)


if __name__ == '__main__':
  unittest.main()
