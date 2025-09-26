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

load("@rules_python//python:defs.bzl", "py_test")
load(
    "//pkg:mappings.bzl",
    "REMOVE_BASE_DIRECTORY",
    "pkg_attributes",
    "pkg_filegroup",
    "pkg_files",
    "strip_prefix",
)
load("//pkg:rpm.bzl", "pkg_rpm")
load("//tests/util:defs.bzl", "directory")

############################################################################
# Test handling of directory outputs
############################################################################

# This package defines tests related to the installation of directory outputs
# (TreeArtifacts) in RPM packages.  The rules in common are:
#
# - The order of incoming pkg_filegroups (and their contents) does not matter
#
# - Each file in a TreeArtifact is installed relative to its position in the
#   TreeArtifact, which itself is relative to the desired install prefix.

package(default_applicable_licenses = ["//:license"])

# The actual test.  It tests whether the contents match a particular desired
# "manifest", most notably with regards to file structure.
py_test(
    name = "layer_with_files",
    srcs = ["rpm_contents_vs_manifest_test.py"],
    data = [":layer_with_files_test_data"],
    env = {"TEST_RPM": "test_dirs_rpm-1.1.1-2222.noarch.rpm"},
    main = "rpm_contents_vs_manifest_test.py",
    tags = [
        "no_windows",  # Windows doesn't have rpm(8) or rpmbuild(8)
    ],
    deps = [
        "//tests/rpm:rpm_util",
        "@rules_python//python/runfiles",
    ],
)

# One cannot simply pass the output of pkg_rpm as runfiles content (#161).  This
# seems to be the easiest way around this problem.
sh_library(
    name = "layer_with_files_test_data",
    testonly = True,
    srcs = [":test_dirs_rpm"],
)

# The RPM (target under test)
pkg_rpm(
    name = "test_dirs_rpm",
    srcs = [
        ":dirs_test_pfg",
    ],
    architecture = "noarch",
    description = """pkg_rpm test rpm description""",
    license = "Apache 2.0",
    release = "2222",
    spec_template = "//tests/rpm:template-test.spec.tpl",
    summary = "pkg_rpm test rpm summary",
    version = "1.1.1",
)

############################################################################
# Test RPM Contents
############################################################################

# Establish a directory structure.  This will end up looking like:

# test_dir/a        (created by a "directory" rule)
# test_dir/b        (created by a "directory" rule)
# test_dir/subdir/c (created by a "directory" rule)
# test_dir/subdir/d (created by a "directory" rule)
# test_dir/e        (created by a genrule)
# test_dir/f        (created by a genrule)

pkg_filegroup(
    name = "dirs_test_pfg",
    srcs = [
        # do not sort
        ":test_dir1_pf",
        ":test_files_from_rules",
    ],
)

pkg_files(
    name = "test_dir1_pf",
    srcs = [":test_dir1"],
    attributes = pkg_attributes(
        group = "root",
        mode = "0644",
        user = "root",
    ),
    strip_prefix = strip_prefix.from_pkg(""),
)

directory(
    name = "test_dir1",
    contents = "test_dir1",
    filenames = [
        "a",
        "b",
        "subdir/c",
        "subdir/d",
    ],
    outdir = "test_dir",
)

pkg_files(
    name = "test_files_from_rules",
    srcs = [":my_files"],
    attributes = pkg_attributes(
        group = "root",
        mode = "0644",
        user = "root",
    ),
    prefix = "test_dir",
)

genrule(
    name = "my_files",
    outs = [
        "e",
        "f",
    ],
    cmd = "touch $(OUTS)",
)

############################################################################
# Regression: Ensure that TreeArtifact installation WRT files is independent of install order
############################################################################

# The actual test, just like before, except the pkg_filegroup input is slightly
# different.
py_test(
    name = "layer_with_files_reversed",
    srcs = ["rpm_contents_vs_manifest_test.py"],
    data = [":layer_with_files_reversed_test_data"],
    env = {"TEST_RPM": "test_dirs_rpm_reversed-1.1.1-2222.noarch.rpm"},
    main = "rpm_contents_vs_manifest_test.py",
    tags = [
        "no_windows",  # Windows doesn't have rpm(8) or rpmbuild(8)
    ],
    deps = [
        "//tests/rpm:rpm_util",
        "@rules_python//python/runfiles",
    ],
)

# One cannot simply pass the output of pkg_rpm as runfiles content (#161).  This
# seems to be the easiest way around this problem.
sh_library(
    name = "layer_with_files_reversed_test_data",
    testonly = True,
    srcs = [":test_dirs_rpm_reversed"],
)

pkg_rpm(
    name = "test_dirs_rpm_reversed",
    srcs = [
        ":dirs_test_pfg_reversed",
    ],
    architecture = "noarch",
    description = """pkg_rpm test rpm description""",
    license = "Apache 2.0",
    release = "2222",
    spec_template = "//tests/rpm:template-test.spec.tpl",
    summary = "pkg_rpm test rpm summary",
    version = "1.1.1",
)

# Like dirs_test_pfg, but the contents are reversed.  One of the major issues
# with #292 was inconsistency due to install order.
pkg_filegroup(
    name = "dirs_test_pfg_reversed",
    srcs = [
        # do not sort
        ":test_files_from_rules",
        ":test_dir1_pf",
    ],
)

############################################################################
# Test that renaming TreeArtifacts to nothing results in the files being dropped
# in their prefix.
############################################################################
py_test(
    name = "treeartifact_ops",
    srcs = ["rpm_treeartifact_ops_test.py"],
    data = [":treeartifact_ops_rpm_test_data"],
    env = {"TEST_RPM": "treeartifact_ops_rpm-1.1.1-2222.noarch.rpm"},
    main = "rpm_treeartifact_ops_test.py",
    tags = [
        "no_windows",  # Windows doesn't have rpm(8) or rpmbuild(8)
    ],
    deps = [
        "//tests/rpm:rpm_util",
        "@rules_python//python/runfiles",
    ],
)

sh_library(
    name = "treeartifact_ops_rpm_test_data",
    testonly = True,
    srcs = [":treeartifact_ops_rpm"],
)

pkg_rpm(
    name = "treeartifact_ops_rpm",
    srcs = [
        ":treeartifact_ops_pfg",
    ],
    architecture = "noarch",
    description = """pkg_rpm test rpm description""",
    license = "Apache 2.0",
    release = "2222",
    spec_template = "//tests/rpm:template-test.spec.tpl",
    summary = "pkg_rpm test rpm summary",
    version = "1.1.1",
)

pkg_filegroup(
    name = "treeartifact_ops_pfg",
    srcs = [
        ":stripped_treeartifact_pf",
    ],
)

pkg_files(
    name = "stripped_treeartifact_pf",
    srcs = [
        ":test_dir1",
    ],
    attributes = pkg_attributes(
        group = "root",
        mode = "0644",
        user = "root",
    ),
    renames = {":test_dir1": REMOVE_BASE_DIRECTORY},
)
