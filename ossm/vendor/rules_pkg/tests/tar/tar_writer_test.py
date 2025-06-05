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
"""Testing for tar_writer."""

import os
import tarfile
import unittest

from python.runfiles import runfiles
from pkg.private.tar import tar_writer
from tests.tar import compressor


class TarFileWriterTest(unittest.TestCase):
  """Testing for TarFileWriter class."""

  def assertTarFileContent(self, tar, content):
    """Assert that tarfile contains exactly the entry described by `content`.

    Args:
        tar: the path to the TAR file to test.
        content: an array describing the expected content of the TAR file.
            Each entry in that list should be a dictionary where each field
            is a field to test in the corresponding TarInfo. For
            testing the presence of a file "x", then the entry could simply
            be `{"name": "x"}`, the missing field will be ignored. To match
            the content of a file entry, use the key "data".
    """
    got_names = []
    with tarfile.open(tar, "r:*") as f:
      for current in f:
        got_names.append(getattr(current, "name"))

    with tarfile.open(tar, "r:*") as f:
      i = 0
      for current in f:
        error_msg = "Extraneous file at end of archive %s: %s" % (
            tar,
            current.name
            )
        self.assertLess(i, len(content), error_msg)
        for k, v in content[i].items():
          if k == "data":
            value = f.extractfile(current).read()
          elif k == "name" and os.name == "nt":
            value = getattr(current, k).replace("\\", "/")
          else:
            value = getattr(current, k)
          error_msg = " ".join([
              "Value `%s` for key `%s` of file" % (value, k),
              "%s in archive %s does" % (current.name, tar),
              "not match expected value `%s`" % v
              ])
          error_msg += str(got_names)
          self.assertEqual(value, v, error_msg)
        i += 1
      if i < len(content):
        self.fail("Missing file %s in archive %s" % (content[i], tar))

  def setUp(self):
    super(TarFileWriterTest, self).setUp()
    self.tempfile = os.path.join(os.environ["TEST_TMPDIR"], "test.tar")
    self.data_files = runfiles.Create()

  def tearDown(self):
    super(TarFileWriterTest, self).tearDown()
    if os.path.exists(self.tempfile):
      os.remove(self.tempfile)

  def testEmptyTarFile(self):
    with tar_writer.TarFileWriter(self.tempfile):
      pass
    self.assertTarFileContent(self.tempfile, [])

  def assertSimpleFileContent(self, names):
    with tar_writer.TarFileWriter(self.tempfile) as f:
      for n in names:
        f.add_file(n, content=n)
    # pylint: disable=g-complex-comprehension
    content = ([{"name": n,
                 "size": len(n.encode("utf-8")),
                 "data": n.encode("utf-8")}
                for n in names])
    self.assertTarFileContent(self.tempfile, content)

  def testAddFile(self):
    self.assertSimpleFileContent(["./a"])
    self.assertSimpleFileContent(["./b"])
    self.assertSimpleFileContent(["./ab"])
    self.assertSimpleFileContent(["./a", "./b"])
    self.assertSimpleFileContent(["./a", "./ab"])
    self.assertSimpleFileContent(["./a", "./b", "./ab"])

  def testDottedFiles(self):
    with tar_writer.TarFileWriter(self.tempfile) as f:
      f.add_file("a")
      f.add_file("/b")
      f.add_file("./c")
      f.add_file("./.d")
      f.add_file("..e")
      f.add_file(".f")
    content = [
        {"name": "a"},
        {"name": "/b"},
        {"name": "./c"},
        {"name": "./.d"},
        {"name": "..e"},
        {"name": ".f"}
    ]
    self.assertTarFileContent(self.tempfile, content)

  def testMergeTar(self):
    content = [
        {"name": "./a", "data": b"a"},
        {"name": "./ab", "data": b"ab"},
    ]
    for ext in [("." + comp if comp else "") for comp in tar_writer.COMPRESSIONS]:
      with tar_writer.TarFileWriter(self.tempfile) as f:
        datafile = self.data_files.Rlocation(
            "rules_pkg/tests/testdata/tar_test.tar" + ext)
        f.add_tar(datafile, name_filter=lambda n: n != "./b")
      self.assertTarFileContent(self.tempfile, content)

  def testMergeTarRelocated(self):
    content = [
        {"name": "foo", "mode": 0o755},
        {"name": "foo/a", "data": b"a"},
        {"name": "foo/ab", "data": b"ab"},
        ]
    with tar_writer.TarFileWriter(self.tempfile, create_parents=True, allow_dups_from_deps=False) as f:
      datafile = self.data_files.Rlocation(
          "rules_pkg/tests/testdata/tar_test.tar")
      f.add_tar(datafile, name_filter=lambda n: n != "./b", prefix="foo")
    self.assertTarFileContent(self.tempfile, content)

  def testDefaultMtimeNotProvided(self):
    with tar_writer.TarFileWriter(self.tempfile) as f:
      self.assertEqual(f.default_mtime, 0)

  def testDefaultMtimeProvided(self):
    with tar_writer.TarFileWriter(self.tempfile, default_mtime=1234) as f:
      self.assertEqual(f.default_mtime, 1234)

  def testPortableMtime(self):
    with tar_writer.TarFileWriter(self.tempfile, default_mtime="portable") as f:
      self.assertEqual(f.default_mtime, 946684800)

  def testPreserveTarMtimesTrueByDefault(self):
    with tar_writer.TarFileWriter(self.tempfile) as f:
      input_tar_path = self.data_files.Rlocation(
          "rules_pkg/tests/testdata/tar_test.tar")
      f.add_tar(input_tar_path)
      input_tar = tarfile.open(input_tar_path, "r")
      for file_name in f.members:
        input_file = input_tar.getmember(file_name)
        output_file = f.tar.getmember(file_name)
        self.assertEqual(input_file.mtime, output_file.mtime)

  def testPreserveTarMtimesFalse(self):
    with tar_writer.TarFileWriter(self.tempfile, preserve_tar_mtimes=False) as f:
      input_tar_path = self.data_files.Rlocation(
          "rules_pkg/tests/testdata/tar_test.tar")
      f.add_tar(input_tar_path)
      for output_file in f.tar:
        self.assertEqual(output_file.mtime, 0)

  def testAddingDirectoriesForFile(self):
    with tar_writer.TarFileWriter(self.tempfile, create_parents=True) as f:
      f.add_file("d/f")
    content = [
        {"name": "d", "mode": 0o755},
        {"name": "d/f"},
    ]
    self.assertTarFileContent(self.tempfile, content)

  def testAddingDirectoriesForFileManually(self):
    with tar_writer.TarFileWriter(self.tempfile, create_parents=True, allow_dups_from_deps=False) as f:
      f.add_file("d", tarfile.DIRTYPE)
      f.add_file("d/f")

      f.add_file("a", tarfile.DIRTYPE)
      f.add_file("a/b", tarfile.DIRTYPE)
      f.add_file("a/b", tarfile.DIRTYPE)
      f.add_file("a/b/", tarfile.DIRTYPE)
      f.add_file("a/b/c/f")

      f.add_file("x/y/f")
      f.add_file("x", tarfile.DIRTYPE)
    content = [
        {"name": "d", "mode": 0o755},
        {"name": "d/f"},
        {"name": "a", "mode": 0o755},
        {"name": "a/b", "mode": 0o755},
        {"name": "a/b/c", "mode": 0o755},
        {"name": "a/b/c/f"},
        {"name": "x", "mode": 0o755},
        {"name": "x/y", "mode": 0o755},
        {"name": "x/y/f"},
    ]
    self.assertTarFileContent(self.tempfile, content)

  def testAddingOnlySpecifiedFiles(self):
    with tar_writer.TarFileWriter(self.tempfile, allow_dups_from_deps=False) as f:
      f.add_file("a", tarfile.DIRTYPE)
      f.add_file("a/b", tarfile.DIRTYPE)
      f.add_file("a/b/", tarfile.DIRTYPE)
      f.add_file("a/b/c/f")
    content = [
        {"name": "a", "mode": 0o755},
        {"name": "a/b", "mode": 0o755},
        {"name": "a/b/c/f"},
    ]
    self.assertTarFileContent(self.tempfile, content)

  def testPackageDirAttribute(self):
    """Tests package_dir of pkg_tar."""
    package_dir = self.data_files.Rlocation(
        "rules_pkg/tests/tar/test_tar_package_dir.tar")
    expected_content = [
        {"name": "my"},
        {"name": "my/package"},
        {"name": "my/package/mylink"},
        {"name": "my/package/nsswitch.conf"},
    ]
    self.assertTarFileContent(package_dir, expected_content)

  def testPackageDirFileAttribute(self):
    """Tests package_dir_file attributes of pkg_tar."""
    package_dir_file = self.data_files.Rlocation(
        "rules_pkg/tests/tar/test_tar_package_dir_file.tar")
    expected_content = [
        {"name": "package"},
        {"name": "package/nsswitch.conf"},
    ]
    self.assertTarFileContent(package_dir_file, expected_content)

  def testCustomCompression(self):
    original = self.data_files.Rlocation(
        "rules_pkg/tests/testdata/tar_test.tar")
    compressed = self.data_files.Rlocation(
        "rules_pkg/tests/tar/test_tar_compression.tar")
    with open(compressed, "rb") as f_in, open(self.tempfile, "wb") as f_out:
      # "Decompress" by skipping garbage bytes
      f_in.seek(len(compressor.GARBAGE))
      f_out.write(f_in.read())

    expected_content = [
        {"name": "./" + x, "data": x.encode("utf-8")} for x in ["a", "b", "ab"]
    ]
    self.assertTarFileContent(original, expected_content)
    self.assertTarFileContent(self.tempfile, expected_content)

  def testAdditionOfDuplicatePath(self):
    expected_content = [
        {"name": "./" + x} for x in ["a", "b", "ab"]] + [
        {"name": "./b", "data": "q".encode("utf-8")}
    ]
    with tar_writer.TarFileWriter(self.tempfile) as f:
      datafile = self.data_files.Rlocation(
        "rules_pkg/tests/testdata/tar_test.tar")
      f.add_tar(datafile)
      f.add_file('./b', content="q")

    self.assertTarFileContent(self.tempfile, expected_content)

  def testAdditionOfArchives(self):

    expected_content = [
        {"name": "./" + x} for x in ["a", "b", "ab", "a", "b", "ab"]
    ]
    with tar_writer.TarFileWriter(self.tempfile) as f:
      datafile = self.data_files.Rlocation(
        "rules_pkg/tests/testdata/tar_test.tar")

      f.add_tar(datafile)
      f.add_tar(datafile)

    self.assertTarFileContent(self.tempfile, expected_content)

  def testOnlyIntermediateParentsInferred(self):
    expected_content = [
      {"name": "./a", "mode": 0o111},
      {"name": "./a/b", "mode": 0o755},
      {"name": "./a/b/c"},
    ]
    with tar_writer.TarFileWriter(self.tempfile, create_parents=True) as f:
      f.add_file('./a', tarfile.DIRTYPE, mode=0o111)
      f.add_file('./a/b/c')

    self.assertTarFileContent(self.tempfile, expected_content)



if __name__ == "__main__":
  unittest.main()
