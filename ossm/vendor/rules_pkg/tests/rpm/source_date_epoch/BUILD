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
load("//pkg:mappings.bzl", "pkg_filegroup", "pkg_files")
load("//pkg:rpm.bzl", "pkg_rpm")

############################################################################
# Test handling the source_date_epoch attribute
############################################################################

package(default_applicable_licenses = ["//:license"])

# The actual test.  It tests whether the contents match a particular desired
# "manifest", most notably with regards to file structure.
py_test(
    name = "source_date_epoch_insource",
    srcs = ["rpm_contents_vs_manifest_test.py"],
    data = [":rpm_sde_insource_data"],
    env = {"TEST_RPM": "rpm_sde_insource-1.1.1-2222.noarch.rpm"},
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
    name = "rpm_sde_insource_data",
    testonly = True,
    srcs = [":rpm_sde_insource"],
)

# The RPM (target under test)
pkg_rpm(
    name = "rpm_sde_insource",
    srcs = [
        ":pfg",
    ],
    architecture = "noarch",
    description = """pkg_rpm test rpm description""",
    license = "Apache 2.0",
    release = "2222",
    # Tue Mar 23 00:00:00 EDT 2021
    source_date_epoch = 1616472000,
    spec_template = "//tests/rpm:template-test.spec.tpl",
    summary = "pkg_rpm test rpm summary",
    version = "1.1.1",
)

pkg_filegroup(
    name = "pfg",
    srcs = [":pf"],
)

pkg_files(
    name = "pf",
    srcs = [":files"],
    prefix = "test_dir",
)

genrule(
    name = "files",
    outs = [
        "a",
        "b",
    ],
    cmd = """
    touch $(OUTS)
    """,
)

############################################################################
# Test handling the source_date_epoch attribute
############################################################################

py_test(
    name = "source_date_epoch_from_file",
    srcs = ["rpm_contents_vs_manifest_test.py"],
    data = [":rpm_sde_fromfile_data"],
    env = {"TEST_RPM": "rpm_sde_fromfile-1.1.1-2222.noarch.rpm"},
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
    name = "rpm_sde_fromfile_data",
    testonly = True,
    srcs = [":rpm_sde_fromfile"],
)

# The RPM (target under test)
pkg_rpm(
    name = "rpm_sde_fromfile",
    srcs = [
        ":pfg",
    ],
    architecture = "noarch",
    description = """pkg_rpm test rpm description""",
    license = "Apache 2.0",
    release = "2222",
    # Tue Mar 23 00:00:00 EDT 2021
    source_date_epoch_file = "epoch.txt",
    spec_template = "//tests/rpm:template-test.spec.tpl",
    summary = "pkg_rpm test rpm summary",
    version = "1.1.1",
)
