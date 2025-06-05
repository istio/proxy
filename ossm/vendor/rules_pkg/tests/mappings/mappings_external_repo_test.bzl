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

"""Tests for file mapping routines in pkg/mappings.bzl"""

load("//pkg:mappings.bzl", "pkg_files", "strip_prefix")
load(":mappings_test.bzl", "pkg_files_contents_test")

##########
# pkg_files tests involving external repositories
##########

# Tests involving external repositories
def _test_pkg_files_extrepo():
    # From external repo root, basenames only
    pkg_files(
        name = "pf_extrepo_strip_all_g",
        srcs = ["@mappings_test_external_repo//pkg:dir/script"],
        tags = ["manual"],
    )

    pkg_files_contents_test(
        name = "pf_extrepo_strip_all",
        target_under_test = ":pf_extrepo_strip_all_g",
        expected_dests = ["extproj.sh", "script"],
    )

    # From external repo root, relative to the "pkg" package
    pkg_files(
        name = "pf_extrepo_strip_from_pkg_g",
        srcs = ["@mappings_test_external_repo//pkg:dir/script"],
        strip_prefix = strip_prefix.from_pkg("dir"),
        tags = ["manual"],
    )
    pkg_files_contents_test(
        name = "pf_extrepo_strip_from_pkg",
        target_under_test = ":pf_extrepo_strip_from_pkg_g",
        expected_dests = [
            "extproj.sh",
            "script",
        ],
    )

    # From external repo root, relative to the "pkg" directory
    pkg_files(
        name = "pf_extrepo_strip_from_root_g",
        srcs = ["@mappings_test_external_repo//pkg:dir/script"],
        strip_prefix = strip_prefix.from_root("pkg"),
        tags = ["manual"],
    )
    pkg_files_contents_test(
        name = "pf_extrepo_strip_from_root",
        target_under_test = ":pf_extrepo_strip_from_root_g",
        expected_dests = ["dir/extproj.sh", "dir/script"],
    )

    native.filegroup(
        name = "extrepo_test_fg",
        srcs = ["@mappings_test_external_repo//pkg:dir/extproj.sh"],
    )

    # Test the case when a have a pkg_files that targets a local filegroup
    # that has files in an external repo.
    pkg_files(
        name = "pf_extrepo_filegroup_strip_from_pkg_g",
        srcs = [":extrepo_test_fg"],
        # Files within filegroups should be considered relative to their
        # destination paths.
        strip_prefix = strip_prefix.from_pkg(""),
    )
    pkg_files_contents_test(
        name = "pf_extrepo_filegroup_strip_from_pkg",
        target_under_test = ":pf_extrepo_filegroup_strip_from_pkg_g",
        expected_dests = ["dir/extproj.sh"],
    )

    # Ditto, except strip from the workspace root instead
    pkg_files(
        name = "pf_extrepo_filegroup_strip_from_root_g",
        srcs = [":extrepo_test_fg"],
        # Files within filegroups should be considered relative to their
        # destination paths.
        strip_prefix = strip_prefix.from_root("pkg"),
    )
    pkg_files_contents_test(
        name = "pf_extrepo_filegroup_strip_from_root",
        target_under_test = ":pf_extrepo_filegroup_strip_from_root_g",
        expected_dests = ["dir/extproj.sh"],
    )

    # Reference a pkg_files in @mappings_test_external_repo
    pkg_files_contents_test(
        name = "pf_pkg_files_in_extrepo",
        target_under_test = "@mappings_test_external_repo//pkg:extproj_script_pf",
        expected_dests = ["usr/bin/dir/extproj.sh"],
    )

# buildifier: disable=unnamed-macro
def mappings_external_repo_analysis_tests():
    """Declare mappings.bzl analysis tests"""
    _test_pkg_files_extrepo()

    native.test_suite(
        name = "pkg_files_external_repo_analysis_tests",
        # We should find a way to get rid of this test list; it would be nice if
        # it could be derived from something else...
        tests = [
            # buildifier: don't sort
            # Tests involving external repositories
            ":pf_extrepo_strip_all",
            ":pf_extrepo_strip_from_pkg",
            ":pf_extrepo_strip_from_root",
            ":pf_extrepo_filegroup_strip_from_pkg",
            ":pf_extrepo_filegroup_strip_from_root",
            ":pf_pkg_files_in_extrepo",
            ":pf_file_rename_to_empty",
            ":pf_directory_rename_to_empty",
            # This one fits into the same category, but can't be aliased, apparently.
            #
            # The main purpose behind it is to verify cases wherein we build a
            # file, but then have it consumed by some remote package.
            "@mappings_test_external_repo//pkg:pf_local_file_in_extrepo",
            # This test is more focused around the verify_archive_test repo but is still
            # based on using something from another repo. In particular, ensuring the
            # verify_archive_test macro can be called properly from a workspace outside
            # of the main one.
            "@mappings_test_external_repo//pkg:external_archive_test",
        ],
    )
