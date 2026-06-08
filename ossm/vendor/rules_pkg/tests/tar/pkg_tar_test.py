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
"""Testing for pkg_tar."""

import os
import tarfile
import unittest

from python.runfiles import runfiles
from pkg.private.tar import tar_writer

PORTABLE_MTIME = 946684800  # 2000-01-01 00:00:00.000 UTC


class PkgTarTest(unittest.TestCase):
  """Testing for pkg_tar rule."""

  def assertTarFileContent(self, file_name, content, verbose=False):
    """Assert that tarfile contains exactly the entry described by `content`.

    Args:
        file_name: the path to the TAR file to test.
        content: an array describing the expected content of the TAR file.
            Each entry in that list should be a dictionary where each field
            is a field to test in the corresponding TarInfo. For
            testing the presence of a file "x", then the entry could simply
            be `{'name': 'x'}`, the missing field will be ignored. To match
            the content of a file entry, use the key 'data'.
    """
    # NOTE: This is portable to Windows. os.path.join('rules_pkg', 'tests',
    # filename) is not.
    file_path = runfiles.Create().Rlocation('rules_pkg/tests/tar/' + file_name)
    got = []
    with tarfile.open(file_path, 'r:*') as f:
      i = 0
      for info in f:
        if verbose:
          print('  >> from tar file:', info.name)
        error_msg = 'Extraneous file at end of archive %s: %s' % (
            file_path,
            info.name
            )
        self.assertLess(i, len(content), error_msg)
        for k, v in content[i].items():
          if k == 'halt':
            return
          elif k == 'data':
            value = f.extractfile(info).read()
          elif k == 'isdir':
            value = info.isdir()
          else:
            value = getattr(info, k)
          if k == 'mode':
            p_value = '0o%o' % value
            p_v = '0o%o' % v
          else:
            p_value = str(value)
            p_v = str(v)
          error_msg = ' '.join([
              'Value `%s` for key `%s` of file' % (p_value, k),
              '%s in archive %s does' % (info.name, file_path),
              'not match expected value `%s`' % p_v
              ])
          self.assertEqual(value, v, error_msg)
          if value != v:
            print(error_msg)
        i += 1
      if i < len(content):
        self.fail('Missing file %s in archive %s of [%s]' % (
            content[i], file_path, ',\n    '.join(got)))

  def test_strip_prefix_empty(self):
    content = [
        {'name': 'nsswitch.conf'},
    ]
    self.assertTarFileContent('test-tar-strip_prefix-empty.tar', content)

  def test_strip_prefix_none(self):
    content = [
        {'name': 'nsswitch.conf'},
    ]
    self.assertTarFileContent('test-tar-strip_prefix-none.tar', content)

  def test_strip_prefix_etc(self):
    content = [
        {'name': 'nsswitch.conf'},
    ]
    self.assertTarFileContent('test-tar-strip_prefix-etc.tar', content)

  def test_strip_prefix_substring(self):
    content = [
        {'name': 'etc', 'isdir': True},
        {'name': 'etc/nsswitch.conf'},
    ]
    self.assertTarFileContent('test-tar-strip_prefix-substring.tar', content)

  def disabled_test_strip_prefix_dot(self):
    content = [
        {'name': 'etc'},
        {'name': 'etc/nsswitch.conf'},
        {'name': 'external'},
        {'name': 'external/rules_python'},
        {'name': 'external/rules_python/python'},
        {'name': 'external/rules_python/python/runfiles'},
        # This is brittle. In old bazel the next file would be
        # external/rules_python/python/runfiles/runfiles.py, but there
        # is now _runfiles_constants.py, first. So this is too brittle.
        {'halt': None},
    ]
    self.assertTarFileContent('test-tar-strip_prefix-dot.tar', content)

  def test_strip_files_dict(self):
    content = [
        {'name': 'not-etc'},
        {'name': 'not-etc/mapped-filename.conf'},
    ]
    self.assertTarFileContent('test-tar-files_dict.tar', content)

  def test_empty_files(self):
    content = [
        {'name': 'a', 'size': 0, 'uid': 0},
        {'name': 'b', 'size': 0, 'uid': 0, 'mtime': PORTABLE_MTIME},
    ]
    self.assertTarFileContent('test-tar-empty_files.tar', content)

  def test_empty_dirs(self):
    content = [
        {'name': 'pmt', 'isdir': True, 'size': 0, 'uid': 0,
         'mtime': PORTABLE_MTIME},
        {'name': 'tmp', 'isdir': True, 'size': 0, 'uid': 0,
         'mtime': PORTABLE_MTIME},
    ]
    self.assertTarFileContent('test-tar-empty_dirs.tar', content)

  def test_mtime(self):
    # Note strange mtime. It is specified in the BUILD file.
    content = [
        {'name': 'nsswitch.conf', 'mtime': 946684740},
    ]
    self.assertTarFileContent('test-tar-mtime.tar', content)

  def test_basic(self):
    # Check the set of 'test-tar-basic-*' smoke test.
    content = [
        {'name': 'etc',
         'uid': 24, 'gid': 42, 'uname': 'tata', 'gname': 'titi'},
        {'name': 'etc/nsswitch.conf',
         'mode': 0o644,
         'uid': 24, 'gid': 42, 'uname': 'tata', 'gname': 'titi'
         },
        {'name': 'usr',
         'uid': 42, 'gid': 24, 'uname': 'titi', 'gname': 'tata'},
        {'name': 'usr/bin'},
        {'name': 'usr/bin/java', 'linkname': '/path/to/bin/java'},
        {'name': 'usr/titi',
         'mode': 0o755,
         'uid': 42, 'gid': 24, 'uname': 'titi', 'gname': 'tata'},
    ]
    for ext in [('.' + comp if comp else '')
                for comp in tar_writer.COMPRESSIONS]:
      with self.subTest(ext=ext):
        self.assertTarFileContent('test-tar-basic-%s.tar%s' % (ext[1:], ext),
                                  content)

  def test_file_inclusion(self):
    content = [
        {'name': 'etc', 'uid': 24, 'gid': 42},
        {'name': 'etc/nsswitch.conf', 'mode': 0o644, 'uid': 24, 'gid': 42},
        {'name': 'usr', 'uid': 42, 'gid': 24},
        {'name': 'usr/bin'},
        {'name': 'usr/bin/java', 'linkname': '/path/to/bin/java'},
        {'name': 'usr/titi', 'mode': 0o755, 'uid': 42, 'gid': 24},
        {'name': 'BUILD'},
    ]
    for ext in [('.' + comp if comp else '')
                for comp in tar_writer.COMPRESSIONS]:
      with self.subTest(ext=ext):
        self.assertTarFileContent('test-tar-inclusion-%s.tar' % ext[1:],
                                  content)

  def test_strip_prefix_empty(self):
    content = [
        {'name': 'level1'},
        {'name': 'level1/some_value'},
        {'name': 'level1/some_value/level3'},
        {'name': 'level1/some_value/level3/BUILD'},
        {'name': 'level1/some_value/level3/mydir'},
    ]
    self.assertTarFileContent('test_tar_package_dir_substitution.tar', content)

  def test_tar_with_long_file_name(self):
    content = [
      {'name': 'file_with_a_ridiculously_long_name_consectetur_adipiscing_elit_fusce_laoreet_lorem_neque_sed_pharetra_erat.txt'}
    ]
    self.assertTarFileContent('test-tar-long-filename.tar', content)

  def test_repackage_file_with_long_name(self):
    content = [
      {'name': 'can_i_repackage_a_file_with_a_long_name'},
      {'name': 'can_i_repackage_a_file_with_a_long_name/file_with_a_ridiculously_long_name_consectetur_adipiscing_elit_fusce_laoreet_lorem_neque_sed_pharetra_erat.txt'}
    ]
    self.assertTarFileContent('test-tar-repackaging-long-filename.tar', content)

  def test_tar_with_tree_artifact(self):
    # (sorted) list of files:
    #  "a/a"
    #  "a/b/c"
    #  "b/c/d"
    #  "b/d"
    #  "b/e"

    content = [
      {'name': 'a_tree', 'isdir': True},
      {'name': 'a_tree/generate_tree', 'isdir': True, 'mode': 0o755},
      {'name': 'a_tree/generate_tree/a', 'isdir': True, 'mode': 0o755},
      {'name': 'a_tree/generate_tree/a/a'},
      {'name': 'a_tree/generate_tree/a/b', 'isdir': True, 'mode': 0o755},
      {'name': 'a_tree/generate_tree/a/b/c'},
      {'name': 'a_tree/generate_tree/b', 'isdir': True, 'mode': 0o755},
      {'name': 'a_tree/generate_tree/b/c', 'isdir': True, 'mode': 0o755},
      {'name': 'a_tree/generate_tree/b/c/d'},
      {'name': 'a_tree/generate_tree/b/d'},
      {'name': 'a_tree/generate_tree/b/e'},
      {'name': 'a_tree/loremipsum.txt'},
    ]
    self.assertTarFileContent('test-tar-tree-artifact.tar', content)

    # Now test against the tree artifact with the dir name stripped.
    noroot_content = []
    for c in content[1:]:  # one less level in tree. Skip first element.
      nc = dict(c)
      nc['name'] = c['name'].replace('/generate_tree', '')
      noroot_content.append(nc)
    self.assertTarFileContent('test-tar-tree-artifact-noroot.tar',
                              noroot_content)

  def test_tar_leading_dotslash(self):
    content = [
      {'name': './loremipsum.txt'},
    ]
    self.assertTarFileContent('test_tar_leading_dotslash.tar', content)


  def test_pkg_tar_with_attributes(self):
    content = [
      {'name': 'foo','uid': 0, 'gid': 1000, 'uname': '', 'gname': ''},
      {'name': 'foo/bar','uid': 0, 'gid': 1000, 'uname': '', 'gname': ''},
      {'name': 'foo/bar/loremipsum.txt','uid': 0, 'gid': 1000, 'uname': '', 'gname': ''},
    ]
    self.assertTarFileContent('test-pkg-tar-with-attributes.tar', content)

  def test_pkg_files_with_attributes(self):
    content = [
      {'name': 'foo','uid': 0, 'gid': 1000, 'uname': 'person', 'gname': 'grp'},
      {'name': 'foo/bar','uid': 0, 'gid': 1000, 'uname': 'person', 'gname': 'grp'},
      {'name': 'foo/bar/loremipsum.txt','uid': 0, 'gid': 1000, 'uname': 'person', 'gname': 'grp'},
    ]
    self.assertTarFileContent('test-pkg-tar-from-pkg-files-with-attributes.tar', content)

  def test_tar_with_tree_artifact_and_strip_prefix(self):
    content = [
      {'name': 'a', 'isdir': True},
      {'name': 'a/a'},
      {'name': 'a/b'},
    ]
    self.assertTarFileContent('test-tree-input-with-strip-prefix.tar', content)

  def test_remap_paths_tree_artifact(self):
    content = [
      {'name': 'a_new_name', 'isdir': True},
      {'name': 'a_new_name/a'},
      {'name': 'a_new_name/rename_me', 'isdir': True},
      {'name': 'a_new_name/rename_me/should_not_rename'},
    ]
    self.assertTarFileContent('test-remap-paths-tree-artifact.tar', content)

  def test_externally_defined_duplicate_structure(self):
    content = [
      {'name': './a'},
      {'name': './b'},
      {'name': './ab'},
      {'name': './ab'},
    ]
    self.assertTarFileContent('test-respect-externally-defined-duplicates.tar', content)

  def test_compression_level(self):
    cases = [
      ('test-tar-compression_level--1.tgz', 179),
      ('test-tar-compression_level-3.tgz', 230),
      ('test-tar-compression_level-6.tgz', 178),
      ('test-tar-compression_level-9.tgz', 167),
      ('test-tar-xz-compression_level--1.tar.xz', 67216),
      ('test-tar-xz-compression_level-3.tar.xz', 67264),
      ('test-tar-xz-compression_level-6.tar.xz', 67216),
      ('test-tar-xz-compression_level-9.tar.xz', 67156),
    ]
    for file_name, expected_size in cases:
      file_path = runfiles.Create().Rlocation('rules_pkg/tests/tar/' + file_name)
      file_size = os.stat(file_path).st_size
      self.assertEqual(file_size, expected_size, 'size error for ' + file_name)

  def test_preserve_mode(self):
    if os.name == 'nt':
      expected_mode = [
        ('test-tar-preserve_mode-False.tar', "0o555"), # chmod 555 = r-x r-x r-x
        ('test-tar-preserve_mode-True.tar', "0o666"),  # chmod 666 = rw- rw- rw-
      ]
    else:
      expected_mode = [
        ('test-tar-preserve_mode-False.tar', "0o555"), # chmod 555 = r-x r-x r-x
        ('test-tar-preserve_mode-True.tar', "0o644"),  # chmod 644 = rw- r-- r--
      ]
    for file_name, expected_mode in expected_mode:
      file_path = runfiles.Create().Rlocation('rules_pkg/tests/tar/' + file_name)
      with tarfile.open(file_path, 'r') as f:
        for member in f.getmembers():
          self.assertEqual(member.name, "hello.txt", "unexpected file name for " + file_name)
          self.assertEqual(member.mode, int(expected_mode, 0), 'file mode not preserved for ' + file_name)

  def test_preserve_mtime(self):
    test_cases = [
      # tar file name, mtime should be equal to PORTABLE_MTIME?
      ('test-tar-preserve_mtime-False.tar', True),
      ('test-tar-preserve_mtime-True.tar', False),
    ]
    for file_name, should_be_equal_to_portable_mtime in test_cases:
      file_path = runfiles.Create().Rlocation('rules_pkg/tests/tar/' + file_name)
      with tarfile.open(file_path, 'r') as f:
        for member in f.getmembers():
          self.assertEqual(member.name, "hello.txt", "unexpected file name for " + file_name)
          if should_be_equal_to_portable_mtime:
            self.assertEqual(member.mtime, PORTABLE_MTIME, "unexpected mtime for " + file_name)
          else:
            self.assertNotEqual(member.mtime, PORTABLE_MTIME, "file mtime not preserved for " + file_name)

if __name__ == '__main__':
  unittest.main()
