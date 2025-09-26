# Copyright 2023 The Bazel Authors. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import os
import pathlib
import re
import sys
import unittest

from lib import main


class ExampleTest(unittest.TestCase):
    def test_coverage_doesnt_shadow_stdlib(self):
        # When we try to import the html module
        import html as html_stdlib

        try:
            import coverage.html as html_coverage
        except ImportError:
            self.skipTest("not running under coverage, skipping")

        self.assertEqual(
            "html",
            f"{html_stdlib.__name__}",
            "'html' from stdlib was not loaded correctly",
        )

        self.assertEqual(
            "coverage.html",
            f"{html_coverage.__name__}",
            "'coverage.html' was not loaded correctly",
        )

        self.assertNotEqual(
            html_stdlib,
            html_coverage,
            "'html' import should not be shadowed by coverage",
        )

    def test_coverage_sys_path(self):
        all_paths = ",\n    ".join(sys.path)

        for i, path in enumerate(sys.path[1:-2]):
            self.assertFalse(
                "/coverage" in path,
                f"Expected {i + 2}th '{path}' to not contain 'coverage.py' paths, "
                f"sys.path has {len(sys.path)} items:\n    {all_paths}",
            )

        first_item, last_item = sys.path[0], sys.path[-1]
        self.assertFalse(
            first_item.endswith("coverage"),
            f"Expected the first item in sys.path '{first_item}' to not be related to coverage",
        )

        # We're trying to make sure that the coverage library added by the
        # toolchain is _after_ any user-provided dependencies. This lets users
        # override what coverage version they're using.
        first_coverage_index = None
        last_user_dep_index = None
        for i, path in enumerate(sys.path):
            if re.search("rules_python.*[~+]pip[~+]", path):
                last_user_dep_index = i
            if first_coverage_index is None and re.search(
                ".*rules_python.*[~+]python[~+].*coverage.*", path
            ):
                first_coverage_index = i

        if os.environ.get("COVERAGE_MANIFEST"):
            self.assertIsNotNone(
                first_coverage_index,
                "Expected to find toolchain coverage, but "
                + f"it was not found.\nsys.path:\n{all_paths}",
            )
            self.assertIsNotNone(
                last_user_dep_index,
                "Expected to find at least one user dep, "
                + "but none were found.\nsys.path:\n{all_paths}",
            )
            # we are running under the 'bazel coverage :test'
            self.assertGreater(
                first_coverage_index,
                last_user_dep_index,
                "Expected coverage provided by the toolchain to be after "
                + "user provided dependencies.\n"
                + f"Found coverage at index: {first_coverage_index}\n"
                + f"Last user dep at index: {last_user_dep_index}\n"
                + f"Full sys.path:\n{all_paths}",
            )
        else:
            self.assertIsNone(
                first_coverage_index,
                "Expected toolchain coverage to not be present\n"
                + f"Found coverage at index: {first_coverage_index}\n"
                + f"Full sys.path:\n{all_paths}",
            )

    def test_main(self):
        self.assertEqual(
            """\
-  -
A  1
B  2
-  -""",
            main([["A", 1], ["B", 2]]),
        )


if __name__ == "__main__":
    unittest.main()
