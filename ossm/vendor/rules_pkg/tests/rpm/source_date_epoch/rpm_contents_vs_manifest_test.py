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

import csv
import io
import os
import unittest

from python.runfiles import runfiles
from tests.rpm import rpm_util

# Tue Mar 23 00:00:00 EDT 2021
EXPECTED_EPOCH = '1616472000'
EXPECTED_RPM_MANIFEST_CSV = """
path,mtime
/test_dir/a,{epoch}
/test_dir/b,{epoch}
""".strip().format(epoch=EXPECTED_EPOCH)


def version_to_string(version):
    return ".".join(str(i) for i in version)


class PkgRpmCompManifest(unittest.TestCase):
    # Support for SOURCE_DATE_EPOCH is only available as of rpm
    # 4.14: http://rpm.org/wiki/Releases/4.14.0
    #
    # TODO(nacl): it would probably be better to ask the rpmbuild(8) binary for
    # this instead, since that's ultimately what's going to make or break this
    # test.
    SOURCE_DATE_EPOCH_MIN_VERSION = (4, 14, 0)

    def rpmBinSupportsSourceDateEpoch(self):
        return self.rpm_bin_version >= self.SOURCE_DATE_EPOCH_MIN_VERSION

    # TODO(nacl) consider making a fixture out of this
    def setUp(self):
        self.runfiles = runfiles.Create()
        self.maxDiff = None

        self.rpm_bin_version = rpm_util.get_rpm_version_as_tuple()

        # These tests will fail on unsupported versions of rpm(8), so skip them
        # in that case.
        if not self.rpmBinSupportsSourceDateEpoch():
            self.skipTest("RPM version too old to support SOURCE_DATE_EPOCH."
                          "  Must be {} or newer (is {})".format(
                              version_to_string(self.SOURCE_DATE_EPOCH_MIN_VERSION),
                              version_to_string(self.rpm_bin_version),
                          ))

        if "TEST_RPM" not in os.environ:
            self.fail("TEST_RPM must be set in the environment, containing "
                      "the name of the RPM to test")

        # Allow for parameterization of this test based on the desired RPM to
        # test.
        self.rpm_file_path = self.runfiles.Rlocation("/".join([
            os.environ["TEST_WORKSPACE"],
            "tests", "rpm", "source_date_epoch",
            # The object behind os.environ is not a dict, and thus doesn't have
            # the "getdefault()" we'd otherwise use here.
            os.environ["TEST_RPM"],
        ]))

    def test_contents_match(self):
        sio = io.StringIO(EXPECTED_RPM_MANIFEST_CSV)
        manifest_reader = csv.DictReader(sio)
        manifest_specs = {r['path']: r for r in manifest_reader}

        rpm_specs = rpm_util.read_rpm_filedata(
            self.rpm_file_path,
            query_tag_map={
                "FILENAMES": "path",
                "FILEMTIMES": "mtime",
            })

        self.assertDictEqual(manifest_specs, rpm_specs)

    # Test if the RPM build time field is set to the provided SOURCE_DATE_EPOCH.
    def test_buildtime_set(self):
        actual_epoch = rpm_util.invoke_rpm_with_queryformat(
            self.rpm_file_path,
            "%{BUILDTIME}",
        )
        self.assertEqual(actual_epoch, EXPECTED_EPOCH)


if __name__ == "__main__":
    unittest.main()
