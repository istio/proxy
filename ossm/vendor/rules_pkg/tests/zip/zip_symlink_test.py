# Copyright 2025 The Bazel Authors. All rights reserved.
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

import unittest

from tests.zip import zip_test_lib


class ZipContentsTests(zip_test_lib.ZipContentsTestBase):

  def test_symlink(self):
    self.assertZipFileContent("test_zip_symlink.zip", [
          {"filename": "BUILD", "islink": False},
          {"filename": "fake_symlink", "islink": True}, # raw symlink -> keep symlink
          {"filename": "outer_BUILD", "islink": False},# nonraw symlink -> copy
    ])


if __name__ == "__main__":
  unittest.main()
