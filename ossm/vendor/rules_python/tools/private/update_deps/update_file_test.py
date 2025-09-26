# Copyright 2023 The Bazel Authors. All rights reserved.
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

from tools.private.update_deps.update_file import replace_snippet, unified_diff


class TestReplaceSnippet(unittest.TestCase):
    def test_replace_simple(self):
        current = """\
Before the snippet

# Start marker
To be replaced
It may have the '# Start marker' or '# End marker' in the middle,
But it has to be in the beginning of the line to mark the end of a region.
# End marker

After the snippet
"""
        snippet = "Replaced"
        got = replace_snippet(
            current=current,
            snippet="Replaced",
            start_marker="# Start marker",
            end_marker="# End marker",
        )

        want = """\
Before the snippet

# Start marker
Replaced
# End marker

After the snippet
"""
        self.assertEqual(want, got)

    def test_replace_indented(self):
        current = """\
Before the snippet

    # Start marker
    To be replaced
    # End marker

After the snippet
"""
        got = replace_snippet(
            current=current,
            snippet="    Replaced",
            start_marker="# Start marker",
            end_marker="# End marker",
        )

        want = """\
Before the snippet

    # Start marker
    Replaced
    # End marker

After the snippet
"""
        self.assertEqual(want, got)

    def test_raises_if_start_is_not_found(self):
        with self.assertRaises(RuntimeError) as exc:
            replace_snippet(
                current="foo",
                snippet="",
                start_marker="start",
                end_marker="end",
            )

        self.assertEqual(exc.exception.args[0], "Start marker 'start' was not found")

    def test_raises_if_end_is_not_found(self):
        with self.assertRaises(RuntimeError) as exc:
            replace_snippet(
                current="start",
                snippet="",
                start_marker="start",
                end_marker="end",
            )

        self.assertEqual(exc.exception.args[0], "End marker 'end' was not found")


class TestUnifiedDiff(unittest.TestCase):
    def test_diff(self):
        give_a = """\
First line
second line
Third line
"""
        give_b = """\
First line
Second line
Third line
"""
        got = unified_diff("filename", give_a, give_b)
        want = """\
--- a/filename
+++ b/filename
@@ -1,3 +1,3 @@
 First line
-second line
+Second line
 Third line"""
        self.assertEqual(want, got)


if __name__ == "__main__":
    unittest.main()
