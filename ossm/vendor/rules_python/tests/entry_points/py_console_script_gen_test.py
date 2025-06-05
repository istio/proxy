#!/usr/bin/env python3
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

import pathlib
import tempfile
import textwrap
import unittest

from python.private.py_console_script_gen import run


class RunTest(unittest.TestCase):
    def setUp(self):
        self.maxDiff = None

    def test_no_console_scripts_error(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            tmpdir = pathlib.Path(tmpdir)
            outfile = tmpdir / "out.py"
            given_contents = (
                textwrap.dedent(
                    """
            [non_console_scripts]
            foo = foo.bar:fizz
            """
                ).strip()
                + "\n"
            )
            entry_points = tmpdir / "entry_points.txt"
            entry_points.write_text(given_contents)

            with self.assertRaises(RuntimeError) as cm:
                run(
                    entry_points=entry_points,
                    out=outfile,
                    console_script=None,
                    console_script_guess="",
                )

        self.assertEqual(
            "The package does not provide any console_scripts in its entry_points.txt",
            cm.exception.args[0],
        )

    def test_no_entry_point_selected_error(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            tmpdir = pathlib.Path(tmpdir)
            outfile = tmpdir / "out.py"
            given_contents = (
                textwrap.dedent(
                    """
            [console_scripts]
            foo = foo.bar:fizz
            """
                ).strip()
                + "\n"
            )
            entry_points = tmpdir / "entry_points.txt"
            entry_points.write_text(given_contents)

            with self.assertRaises(RuntimeError) as cm:
                run(
                    entry_points=entry_points,
                    out=outfile,
                    console_script=None,
                    console_script_guess="bar-baz",
                )

        self.assertEqual(
            "Tried to guess that you wanted 'bar-baz', but could not find it. Please select one of the following console scripts: foo",
            cm.exception.args[0],
        )

    def test_incorrect_entry_point(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            tmpdir = pathlib.Path(tmpdir)
            outfile = tmpdir / "out.py"
            given_contents = (
                textwrap.dedent(
                    """
            [console_scripts]
            foo = foo.bar:fizz
            bar = foo.bar:buzz
            """
                ).strip()
                + "\n"
            )
            entry_points = tmpdir / "entry_points.txt"
            entry_points.write_text(given_contents)

            with self.assertRaises(RuntimeError) as cm:
                run(
                    entry_points=entry_points,
                    out=outfile,
                    console_script="baz",
                    console_script_guess="",
                )

        self.assertEqual(
            "The console_script 'baz' was not found, only the following are available: bar, foo",
            cm.exception.args[0],
        )

    def test_a_single_entry_point(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            tmpdir = pathlib.Path(tmpdir)
            given_contents = (
                textwrap.dedent(
                    """
            [console_scripts]
            foo = foo.bar:baz
            """
                ).strip()
                + "\n"
            )
            entry_points = tmpdir / "entry_points.txt"
            entry_points.write_text(given_contents)
            out = tmpdir / "foo.py"

            run(
                entry_points=entry_points,
                out=out,
                console_script=None,
                console_script_guess="foo",
            )

            got = out.read_text()

        want = textwrap.dedent(
            """\
        import sys

        # See @rules_python//python/private:py_console_script_gen.py for explanation
        if getattr(sys.flags, "safe_path", False):
            # We are running on Python 3.11 and we don't need this workaround
            pass
        elif ".runfiles" not in sys.path[0]:
            sys.path = sys.path[1:]

        try:
            from foo.bar import baz
        except ImportError:
            entries = "\\n".join(sys.path)
            print("Printing sys.path entries for easier debugging:", file=sys.stderr)
            print(f"sys.path is:\\n{entries}", file=sys.stderr)
            raise

        if __name__ == "__main__":
            sys.exit(baz())
        """
        )
        self.assertEqual(want, got)

    def test_a_second_entry_point_class_method(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            tmpdir = pathlib.Path(tmpdir)
            given_contents = (
                textwrap.dedent(
                    """
            [console_scripts]
            foo = foo.bar:Bar.baz
            bar = foo.baz:Bar.baz
            """
                ).strip()
                + "\n"
            )
            entry_points = tmpdir / "entry_points.txt"
            entry_points.write_text(given_contents)
            out = tmpdir / "out.py"

            run(
                entry_points=entry_points,
                out=out,
                console_script="bar",
                console_script_guess="",
            )

            got = out.read_text()

        self.assertRegex(got, "from foo\.baz import Bar")
        self.assertRegex(got, "sys\.exit\(Bar\.baz\(\)\)")


if __name__ == "__main__":
    unittest.main()
