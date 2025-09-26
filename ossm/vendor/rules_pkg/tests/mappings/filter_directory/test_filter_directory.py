#!/usr/bin/env python3

# Copyright 2021 The Bazel Authors. All rights reserved.
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

"""Negative tests for the filter_directory script

For a discussion as for why this needs to exist, see """

import pathlib
import os
import sys
import tempfile
import unittest

from pkg import filter_directory
from python.runfiles import runfiles


# TODO: These tests are largely to ensure that filter_directory fails, but it
# does not check _why_ they fail.
#
# This would involve changing how filter_directory returns errors, or maybe
# restructuring it to make it unit-testable.
#
# Regardless, this would be significantly more code, and is not yet attempted.
class FilterDirectoryInternalTest(unittest.TestCase):

    def setUp(self):
        self.indir = tempfile.TemporaryDirectory(dir=os.environ["TEST_TMPDIR"])
        self.outdir = tempfile.TemporaryDirectory(dir=os.environ["TEST_TMPDIR"])
        indir_path = pathlib.Path(self.indir.name)

        (indir_path / "root").mkdir()
        (indir_path / "root" / "a").open(mode='w').close()
        (indir_path / "root" / "b").open(mode='w').close()
        (indir_path / "root" / "subdir").mkdir()
        (indir_path / "root" / "subdir" / "c").open(mode='w').close()
        (indir_path / "root" / "subdir" / "d").open(mode='w').close()

    def tearDown(self):
        self.indir.cleanup()
        self.outdir.cleanup()

    ###########################################################################
    # Test helpers
    ###########################################################################
    def callFilterDirectory(self,
                            prefix=None,        # str
                            strip_prefix=None,  # str
                            renames=None,       # list of tuple
                            exclusions=None):    # list
        args = []
        if prefix:
            args.append("--prefix={}".format(prefix))
        if strip_prefix:
            args.append("--strip_prefix={}".format(prefix))
        if renames:
            args.extend(["--rename={}={}".format(dest, src) for dest, src in renames])
        if exclusions:
            args.extend(["--exclude={}".format(e) for e in exclusions])

        args.append(self.indir.name)
        args.append(self.outdir.name)

        try:
            filter_directory.main(args)
            return 0  # Success
        except SystemExit as e:
            return e.code

    def assertFilterDirectoryFails(self, message=None, **kwargs):
        self.assertNotEqual(self.callFilterDirectory(**kwargs), 0, message)

    def assertFilterDirectorySucceeds(self, message=None, **kwargs):
        self.assertEqual(self.callFilterDirectory(**kwargs), 0, message)

    ###########################################################################
    # Actual tests
    ###########################################################################

    def test_base(self):
        # Simply verify that the "null" transformation works
        self.assertFilterDirectorySucceeds()

    def test_invalid_prefixes(self):
        self.assertFilterDirectoryFails(
            prefix="/absolute/path",
            message="--prefix with absolute paths should be rejected",
        )

        self.assertFilterDirectoryFails(
            prefix="/absolute/path",
            message="--prefix with paths outside the destroot should be rejected",
        )

    def test_invalid_strip_prefixes(self):
        self.assertFilterDirectoryFails(
            strip_prefix="invalid",
            message="--strip_prefix that does not apply anywhere should be rejected",
        )

        self.assertFilterDirectoryFails(
            strip_prefix="subdir",
            message="--strip_prefix that does not apply everywhere should be rejected",
        )

    def test_invalid_excludes(self):
        self.assertFilterDirectoryFails(
            exclusions=["root/a", "foo"],
            message="--exclude's that are unused should be rejected",
        )

    def test_invalid_renames(self):
        self.assertFilterDirectoryFails(
            renames=[("../outside", "root/a")],
            message="--rename's with paths outside the destroot should be rejected",
        )

        # Can't rename files to outputs that already exist
        self.assertFilterDirectoryFails(
            renames=[("root/a", "root/subdir/c")],
            message="--rename's that clobber other output files should be rejected",
        )

        # This is unreachable from the bazel rule, but it's worth double-checking.
        #
        # Can't rename multiple files to the same destination
        self.assertFilterDirectoryFails(
            renames=[("root/a", "root/subdir/c"), ("root/a", "root/subdir/d")],
            message="Multiple --rename's to the same destination should be rejected.",
        )

        # Can't rename files twice
        self.assertFilterDirectoryFails(
            renames=[("bar", "root/a"), ("foo", "root/a")],
            message="--rename's that attempt to rename the same source twice should be rejected",
        )

    def test_invalid_interactions(self):
        # Renames are supposed to occur after exclusions, the rename here should
        # thus be unused.
        self.assertFilterDirectoryFails(
            renames=[("foo", "root/a")],
            exclusions=["root/a"],
            message="--rename's of excluded files should be rejected",
        )

        # strip_prefix and renames can cause collisions, check that they they're
        # detected.
        self.assertFilterDirectoryFails(
            strip_prefix="root",  # valid
            renames=[("a", "root/subdir/c")],  # Since we're stripping "root/"
                                               # from "root/a", this should
                                               # cause an output collision.
            message="--rename's to paths adjusted by strip_prefix should be rejected",
        )

if __name__ == "__main__":
    unittest.main()
