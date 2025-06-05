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
import os
import sys
import unittest
import zipfile

from python.runfiles import runfiles
from tests.zip import zip_test_lib

HELLO_CRC = 2069210904
LOREM_CRC = 2178844372
EXECUTABLE_CRC = 342626072


class ZipContentsTests(zip_test_lib.ZipContentsTestBase):

  def test_empty(self):
    self.assertZipFileContent("test_zip_empty.zip", [])

  def test_basic(self):
    if os.name == "nt":
      expect = [
          {"filename": "an_executable.exe", "isexe": True},
          {"filename": "foodir/", "isdir": True, "attr": 0o711},
          {"filename": "hello.txt", "crc": HELLO_CRC},
          {"filename": "loremipsum.txt", "crc": LOREM_CRC},
          {"filename": "usr/", "isdir": True, "attr": 0o755},
          {"filename": "usr/bin/", "isdir": True, "attr": 0o755},
          {"filename": "usr/bin/foo", "isexe": True, "data": "/usr/local/foo/foo.real"},
      ]
    else:
      expect = [
          {"filename": "an_executable", "isexe": True},
          {"filename": "foodir/", "isdir": True, "attr": 0o711},
          {"filename": "hello.txt", "crc": HELLO_CRC},
          {"filename": "loremipsum.txt", "crc": LOREM_CRC},
          {"filename": "usr/", "isdir": True, "attr": 0o755},
          {"filename": "usr/bin/", "isdir": True, "attr": 0o755},
          {"filename": "usr/bin/foo", "isexe": True, "data": "/usr/local/foo/foo.real"},
      ]

    self.assertZipFileContent("test_zip_basic.zip", expect)

  def test_timestamp(self):
    self.assertZipFileContent("test_zip_timestamp.zip", [
        {"filename": "hello.txt", "crc": HELLO_CRC, "timestamp": 1234567890},
    ])

  def test_permissions(self):
    self.assertZipFileContent("test_zip_permissions.zip", [
        {
            "filename": "executable.sh",
            "crc": EXECUTABLE_CRC,
            "timestamp": 1234567890,
            "attr": 0o644,
        }
    ])

  def test_package_dir(self):
    self.assertZipFileContent("test_zip_package_dir0.zip", [
        {"filename": "abc/", "isdir": True, "attr": 0o755},
        {"filename": "abc/def/", "isdir": True, "attr": 0o755},
        {"filename": "abc/def/hello.txt", "crc": HELLO_CRC},
        {"filename": "abc/def/loremipsum.txt", "crc": LOREM_CRC},
        {"filename": "abc/def/mylink", "attr": 0o777},
    ])

  def test_package_dir_substitution(self):
    self.assertZipFileContent("test_zip_package_dir_substitution.zip", [
        {"filename": "level1/", "isdir": True, "attr": 0o755},
        {"filename": "level1/some_value/", "isdir": True, "attr": 0o755},
        {"filename": "level1/some_value/level3/", "isdir": True, "attr": 0o755},
        {"filename": "level1/some_value/level3/hello.txt", "crc": HELLO_CRC},
        {"filename": "level1/some_value/level3/loremipsum.txt", "crc": LOREM_CRC},
    ])

  def test_zip_strip_prefix_empty(self):
    self.assertZipFileContent("test-zip-strip_prefix-empty.zip", [
        {"filename": "loremipsum.txt", "crc": LOREM_CRC},
    ])

  def test_zip_strip_prefix_none(self):
    self.assertZipFileContent("test-zip-strip_prefix-none.zip", [
        {"filename": "loremipsum.txt", "crc": LOREM_CRC},
    ])

  def test_zip_strip_prefix_zipcontent(self):
    self.assertZipFileContent("test-zip-strip_prefix-zipcontent.zip", [
        {"filename": "loremipsum.txt", "crc": LOREM_CRC},
    ])

  def test_zip_strip_prefix_dot(self):
    self.assertZipFileContent("test-zip-strip_prefix-dot.zip", [
        {"filename": "zipcontent/", "isdir": True, "attr": 0o755},
        {"filename": "zipcontent/loremipsum.txt", "crc": LOREM_CRC},
    ])

  def test_zip_tree(self):
    self.assertZipFileContent("test_zip_tree.zip", [
        {"filename": "generate_tree/", "isdir": True, "attr": 0o755},
        {"filename": "generate_tree/a/", "isdir": True, "attr": 0o755},
        {"filename": "generate_tree/a/a"},
        {"filename": "generate_tree/a/b/", "isdir": True, "attr": 0o755},
        {"filename": "generate_tree/a/b/c"},
        {"filename": "generate_tree/b/", "isdir": True, "attr": 0o755},
        {"filename": "generate_tree/b/c/", "isdir": True, "attr": 0o755},
        {"filename": "generate_tree/b/c/d"},
        {"filename": "generate_tree/b/d"},
        {"filename": "generate_tree/b/e"},
    ])

  def test_compression_deflated(self):
    if sys.version_info >= (3, 7):
      self.assertZipFileContent("test_zip_deflated_level_3.zip", [
            {"filename": "loremipsum.txt", "crc": LOREM_CRC, "size": 312},
      ])
    else:
      # Python 3.6 doesn't support setting compresslevel, so the file size differs
      self.assertZipFileContent("test_zip_deflated_level_3.zip", [
            {"filename": "loremipsum.txt", "crc": LOREM_CRC, "size": 309},
      ])

  def test_compression_bzip2(self):
    self.assertZipFileContent("test_zip_bzip2.zip", [
          {"filename": "loremipsum.txt", "crc": LOREM_CRC, "size": 340},
    ])

  def test_compression_lzma(self):
    self.assertZipFileContent("test_zip_lzma.zip", [
          {"filename": "loremipsum.txt", "crc": LOREM_CRC, "size": 378},
    ])

  def test_compression_stored(self):
    self.assertZipFileContent("test_zip_stored.zip", [
          {"filename": "loremipsum.txt", "crc": LOREM_CRC, "size": 543},
    ])



if __name__ == "__main__":
  unittest.main()
