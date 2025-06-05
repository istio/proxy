# Copyright 2024 The Bazel Authors. All rights reserved.
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
import sys
import unittest


class NoUnsafePathsTest(unittest.TestCase):
    def test_no_unsafe_paths_in_search_path(self):
        # Based on sys.path documentation, the first item added is the zip
        # archive
        # (see: https://docs.python.org/3/library/sys_path_init.html)
        #
        # We can use this as a marker to verify that during bootstrapping,
        # (1) no unexpected paths were prepended, and (2) no paths were
        # accidentally dropped.
        #
        major, minor, *_ = sys.version_info
        archive = f"python{major}{minor}.zip"

        # < Python 3.11 behaviour
        if (major, minor) < (3, 11):
            # Because of https://github.com/bazelbuild/rules_python/blob/0.39.0/python/private/stage2_bootstrap_template.py#L415-L436
            self.assertEqual(os.path.dirname(sys.argv[0]), sys.path[0])
            self.assertEqual(os.path.basename(sys.path[1]), archive)
        # >= Python 3.11 behaviour
        else:
            self.assertEqual(os.path.basename(sys.path[0]), archive)


if __name__ == '__main__':
    unittest.main()