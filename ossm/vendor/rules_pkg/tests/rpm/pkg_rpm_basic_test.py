#!/usr/bin/env python3

# Copyright 2020 The Bazel Authors. All rights reserved.
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
import subprocess
import csv
import io
import os

from python.runfiles import runfiles
from tests.rpm import rpm_util

# This provides some tests for built RPMs, mostly by taking the built RPM and
# running rpm queries on it.
#
# Useful reading:
#
# - RPM queryformat documentation (shortish):
#   https://rpm.org/user_doc/query_format.html
#
# - In-depth RPM query documentation:
#   http://ftp.rpm.org/max-rpm/s1-rpm-query-parts.html
#
# - Specifically, about the --qf/--queryformat syntax:
#   http://ftp.rpm.org/max-rpm/s1-rpm-query-parts.html#S3-RPM-QUERY-QUERYFORMAT-OPTION
#
# - --queryformat tags list: http://ftp.rpm.org/max-rpm/ch-queryformat-tags.html
#
class PkgRpmBasicTest(unittest.TestCase):
    def setUp(self):
        self.runfiles = runfiles.Create()
        self.test_rpm_path = self.runfiles.Rlocation(
            "rules_pkg/tests/rpm/test_rpm-1.1.1-2222.noarch.rpm")
        self.test_rpm_direct_path = self.runfiles.Rlocation(
            "rules_pkg/tests/rpm/test_rpm_direct-1.1.1-2222.noarch.rpm")
        self.test_rpm_bzip2_path = self.runfiles.Rlocation(
            "rules_pkg/tests/rpm/test_rpm_bzip2-1.1.1-2222.noarch.rpm")
        self.test_rpm_scriptlets_files_path = self.runfiles.Rlocation(
            "rules_pkg/tests/rpm/test_rpm_scriptlets_files-1.1.1-2222.noarch.rpm")
        self.test_rpm_release_version_files = self.runfiles.Rlocation(
            "rules_pkg/tests/rpm/test_rpm_release_version_files-.noarch.rpm")
        self.test_rpm_epoch = self.runfiles.Rlocation(
            "rules_pkg/tests/rpm/test_rpm_epoch-1.1.1-2222.noarch.rpm")
        self.maxDiff = None

    def test_scriptlet_content(self):
        expected = b"""\
preinstall scriptlet (using /bin/sh):
echo pre
postinstall scriptlet (using /bin/sh):
echo post
preuninstall scriptlet (using /bin/sh):
echo preun
postuninstall scriptlet (using /bin/sh):
echo postun
posttrans scriptlet (using /bin/sh):
echo posttrans
"""

        for path in (self.test_rpm_path, self.test_rpm_scriptlets_files_path):
            output = subprocess.check_output(["rpm", "-qp", "--scripts", path])
            self.assertEqual(output, expected)

    def test_basic_headers(self):
        common_fields = {
            "VERSION": b"1.1.1",
            "RELEASE": b"2222",
            "ARCH": b"noarch",
            "GROUP": b"Unspecified",
            "SUMMARY": b"pkg_rpm test rpm summary",
        }
        for rpm, fields in [
            (self.test_rpm_path, {"NAME": b"test_rpm"}),
            (self.test_rpm_release_version_files, {"NAME": b"test_rpm_release_version_files"}),
            (self.test_rpm_epoch, {"NAME": b"test_rpm_epoch", "EPOCH": b"1"}),
        ]:
            fields.update(common_fields)
            for fieldname, expected in fields.items():
                output = subprocess.check_output([
                    "rpm", "-qp", "--queryformat", "%{" + fieldname + "}",
                    rpm,
                ])

                self.assertEqual(
                    output, expected,
                    "RPM Tag {} does not match expected value".format(fieldname))

    def test_contents(self):
        manifest_file = self.runfiles.Rlocation(
            "rules_pkg/tests/rpm/manifest.csv")
        manifest_specs = {}
        with open(manifest_file, "r", newline='', encoding="utf-8") as fh:
            manifest_reader = csv.DictReader(fh)
            manifest_specs = {r['path']: r for r in manifest_reader}

        rpm_specs = rpm_util.read_rpm_filedata(self.test_rpm_path)

        self.assertDictEqual(manifest_specs, rpm_specs)
        # Transitively, this one should work too -- doesn't use pkg_filegroup
        # directly.
        rpm_direct_specs = rpm_util.read_rpm_filedata(self.test_rpm_direct_path)
        self.assertDictEqual(rpm_specs, rpm_direct_specs)

    def test_preamble_metadata(self):
        metadata_prefix = "rules_pkg/tests/rpm"

        rpm_filename = os.path.basename(self.test_rpm_path)
        rpm_basename = os.path.splitext(rpm_filename)[0]

        # Tuples of:
        # Metadata name, RPM Tag prefix, exclusion list (currently only support "startswith")
        #
        # The exclusions should probably be regexps at some point, but right
        # now, our job is relatively easy.  They only operate on the
        # "capability" portion of the tag.
        test_md = [
            ("conflicts", "CONFLICT", []),
            # rpm packages implicitly provide themselves, something like:
            # "test_rpm = 1.1.1-2222".  We don't bother testing this, since we
            # don't take direct action to specify it.
            ("provides",  "PROVIDE",  [rpm_basename]),
            # Skip rpmlib-related requirements; they are often dependent on the
            # version of `rpm` we are using.
            ("requires",  "REQUIRE",  ["rpmlib"]),
        ]
        for (mdtype, tag, exclusions) in test_md:
            md_file = self.runfiles.Rlocation(
                os.path.join(metadata_prefix, mdtype + ".csv"))

            with open(md_file, "r", newline='', encoding="utf-8") as fh:
                md_reader = csv.DictReader(fh, delimiter=':')
                # I heard you like functional programming ;)
                #
                # This produces a list of outputs whenever the "capability"
                # attribute starts with any of the values in the "exclusions"
                # list.
                md_specs_unsorted = [line for line in md_reader
                                     if not any(line['capability'].startswith(e)
                                                for e in exclusions)]
                # And this sorts it, ordering by the sorted "association list"
                # form of the dictionary.
                #
                # The sorting of the key values is not necessary with versions
                # of python3 (3.5+, I believe) that have dicts maintain
                # insertion order.
                md_specs = sorted(md_specs_unsorted,
                                  key = lambda x: sorted(x.items()))


            # This typically becomes the equivalent of:
            #
            #   '[%{PROVIDENEVRS};%{PROVIDEFLAGS:deptype}\n]'
            #
            # as passed to `rpm --queryformat`
            rpm_queryformat = (
                # NEVRS = Name Epoch Version Release (plural), which look something like:
                #   rpmlib(CompressedFileNames) <= 3.0.4-1
                # or:
                #   bash
                "[%{{{tag}NEVRS}}"
                # Flags associated with the dependency type.  This used to
                # evaluate in what "sense" the dependency was added.
                #
                # Values often include things like:
                #
                # - "interp" for scriptlet interpreter dependencies
                # - "postun" for dependencies of the "postun" scriptlet
                # - "manual" for values that are explicitly specified
                ":%{{{tag}FLAGS!deptype}}"
                "\n]"
            ).format(tag = tag)

            rpm_queryformat_fieldnames = [
                "capability",
                "sense",
            ]

            rpm_output = subprocess.check_output(
                ["rpm", "-qp", "--queryformat", rpm_queryformat, self.test_rpm_path])

            sio = io.StringIO(rpm_output.decode('utf-8'))
            rpm_output_reader = csv.DictReader(
                sio, delimiter='!', fieldnames=rpm_queryformat_fieldnames)

            # Get everything in the same order as the read-in metadata file
            rpm_outputs_filtered_unsorted = [line for line in rpm_output_reader
                                             if not any(line['capability'].startswith(e)
                                                        for e in exclusions)]

            rpm_outputs_filtered = sorted(rpm_outputs_filtered_unsorted, key = lambda x: sorted(x.items()))

            for expected, actual in zip(md_specs, rpm_outputs_filtered):
                self.assertDictEqual(expected, actual,
                                     msg="{} metadata discrepancy".format(mdtype))

    def test_compression_none_provided(self):
        # Test when we don't provide "binary_payload_compression" to pkg_rpm
        my_rpm = self.test_rpm_path
        rpm_output = subprocess.check_output(
            ["rpm", "-qp", "--queryformat", "%{PAYLOADCOMPRESSOR}", my_rpm])
        sio = io.StringIO(rpm_output.decode('utf-8'))
        actual_compressor = sio.read()
        # `bzip2` compression was, AFAICT, never the default for rpmbuild(8),
        # and never will be, so this should be fine.
        self.assertNotEqual(actual_compressor, 'bzip2')

    def test_compression_passthrough(self):
        # Test when we provide "binary_payload_compression" to pkg_rpm
        my_rpm = self.test_rpm_bzip2_path
        rpm_output = subprocess.check_output(
            ["rpm", "-qp", "--queryformat", "%{PAYLOADCOMPRESSOR}", my_rpm])
        sio = io.StringIO(rpm_output.decode('utf-8'))
        actual_compressor = sio.read()
        self.assertEqual(actual_compressor, 'bzip2')

    def test_mtimes(self):
        # Modification times should be something close to when the package was
        # built.  Since we do not know when the package was actually built, we
        # can just check for something that is non-zero.
        #
        # See also #486.

        filedata = rpm_util.read_rpm_filedata(
            self.test_rpm_path,
            query_tag_map={
                "FILENAMES": "path",
                "FILEMTIMES": "mtime",
            }
        )

        self.assertNotEqual(
            len(filedata),
            0,
            "rpm_util.read_rpm_filedata() produced no output"
        )

        filedata_shortened = {
            path: int(data["mtime"])
            for path, data in filedata.items()
        }

        files_with_zero_mtimes = dict(filter(
            lambda kv: kv[1] == 0,
            filedata_shortened.items(),
        ))

        self.assertDictEqual(
            files_with_zero_mtimes,
            {},
            "No files should have zero mtimes without source_date_epoch or source_date_epoch_file")


if __name__ == "__main__":
    unittest.main()
