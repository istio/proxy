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

"""Test that the rules_pkg distribution is usable."""

import os
import re
import subprocess
import unittest

from python.runfiles import runfiles
from pkg.releasing import release_tools
from distro import release_version

_VERBOSE = True


class PackagingTest(unittest.TestCase):
  """Test the distribution packaging."""

  def setUp(self):
    self.data_files = runfiles.Create()
    self.source_repo = 'rules_pkg'
    self.dest_repo = 'not_named_rules_pkg'
    self.version = release_version.RELEASE_VERSION

  def testVersionsMatch(self):
    """version.bzl must match MODULE.bazel"""
    module_bazel_path = self.data_files.Rlocation(
          'rules_pkg/distro/MODULE.bazel')
    with open(module_bazel_path, encoding="utf-8") as inp:
      want = 'version = "%s"' % self.version
      content = inp.read()
      if _VERBOSE:
        print('=== Expect', want)
      m = re.search(
          r"""module\([^)]+\)""",
          content,
          flags=re.MULTILINE|re.DOTALL)
      self.assertTrue(m)
      got = m.group()
      self.assertIn(want, got, 'Expected <%s>, got <%s>' % (want, got))

  def testBuild(self):
    # Set up a fresh Bazel workspace using the currently build repo.
    tempdir = os.path.join(os.environ['TEST_TMPDIR'], 'build')
    if not os.path.exists(tempdir):
      os.makedirs(tempdir)
    with open(os.path.join(tempdir, 'WORKSPACE'), 'w') as workspace:
      file_name = release_tools.package_basename(self.source_repo, self.version)
      # The code looks wrong, but here is why it is correct.
      # - Rlocation requires '/' as path separators, not os.path.sep.
      # - When we read the file, the path must use os.path.sep
      local_path = self.data_files.Rlocation(
          'rules_pkg/distro/' + file_name).replace('/', os.path.sep)
      sha256 = release_tools.get_package_sha256(local_path)
      workspace_content = '\n'.join((
        'workspace(name = "test_rules_pkg_packaging")',
        release_tools.workspace_content(
            'file://%s' % local_path, self.source_repo, sha256,
            rename_repo=self.dest_repo,
            deps_method='rules_pkg_dependencies'
        )
      ))
      workspace.write(workspace_content)
      if _VERBOSE:
        print('=== WORKSPACE ===')
        print(workspace_content)

    # We do a little dance of renaming *.tpl to *, mostly so that we do not
    # have a BUILD file in testdata, which would create a package boundary.
    def CopyTestFile(source_name, dest_name):
      source_path = self.data_files.Rlocation(
          'rules_pkg/distro/testdata/' + source_name)
      with open(source_path) as inp:
        with open(os.path.join(tempdir, dest_name), 'w') as out:
          content = inp.read()
          out.write(content)

    CopyTestFile('BUILD.tpl', 'BUILD')

    os.chdir(tempdir)
    build_result = subprocess.check_output(['bazel', 'build', '--enable_workspace', ':dummy_tar'])
    if _VERBOSE:
      print('=== Build Result ===')
      print(build_result)

    # TODO(aiuto): Find tar in a disciplined way
    content = subprocess.check_output(
        ['tar', 'tzf', 'bazel-bin/dummy_tar.tar.gz'])
    self.assertEqual(b'etc/\netc/BUILD\n', content)


if __name__ == '__main__':
  unittest.main()
