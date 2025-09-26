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
# -*- coding: utf-8 -*-

load("@bazel_skylib//rules:build_test.bzl", "build_test")
load("@bazel_skylib//rules:diff_test.bzl", "diff_test")
load("@rules_python//python:defs.bzl", "py_library", "py_test")
load(
    "//pkg:mappings.bzl",
    "pkg_attributes",
    "pkg_filegroup",
    "pkg_files",
    "pkg_mkdirs",
    "pkg_mklink",
)
load("//pkg:rpm.bzl", "pkg_rpm", "pkg_sub_rpm")
load("analysis_tests.bzl", "analysis_tests")
load("toolchain_tests.bzl", "create_toolchain_analysis_tests")

############################################################################
# analysis tests
############################################################################

package(default_applicable_licenses = ["//:license"])

analysis_tests(name = "analysis_tests")

create_toolchain_analysis_tests()

exports_files(
    ["template-test.spec.tpl"],
    visibility = [":__subpackages__"],
)

############################################################################
# pkg_filegroups for testing
############################################################################

filegroup(
    name = "ars",
    srcs = [
        "//tests:testdata/a.ar",
        "//tests:testdata/a_ab.ar",
        "//tests:testdata/a_b.ar",
        "//tests:testdata/a_b_ab.ar",
        "//tests:testdata/ab.ar",
        "//tests:testdata/b.ar",
        "//tests:testdata/empty.ar",
    ],
)

pkg_files(
    name = "ars_pf",
    srcs = [
        ":ars",
    ],
    attributes = pkg_attributes(
        group = "root",
        mode = "0755",
        user = "root",
    ),
    prefix = "/test",
)

genrule(
    name = "config_empty",
    outs = ["config.txt"],
    cmd = "touch $@",
)

pkg_files(
    name = "config_file",
    srcs = [":config_empty"],
    attributes = pkg_attributes(
        group = "root",
        mode = "0644",
        rpm_filetag = "%config(missingok, noreplace)",
        user = "root",
    ),
)

pkg_mkdirs(
    name = "var_log_foo",
    attributes = pkg_attributes(
        group = "root",
        mode = "0755",
        user = "root",
    ),
    dirs = ["/var/log/foo"],
)

pkg_mklink(
    name = "test_links",
    attributes = pkg_attributes(
        group = "root",
        mode = "0777",
        user = "root",
    ),
    link_name = "/usr/bin/link-name",
    target = "/usr/bin/link-target",
)

pkg_filegroup(
    name = "test_pfg",
    # Keep this list in sync with the values of "srcs" in "test_rpm_direct",
    # below
    srcs = [
        ":ars_pf",
        ":config_file",
        ":test_links",
        ":var_log_foo",
    ],
)

############################################################################
# scriptlets for testing
############################################################################

_POST_SCRIPTLET = "echo post"

_POSTUN_SCRIPTLET = "echo postun"

_PRE_SCRIPTLET = "echo pre"

_PREUN_SCRIPTLET = "echo preun"

_POSTTRANS_SCRIPTLET = "echo posttrans"

[
    genrule(
        name = name,
        outs = ["{}.sh".format(name)],
        cmd = "echo '{}' > $@".format(content),
    )
    for name, content in [
        ("post", _POST_SCRIPTLET),
        ("postun", _POSTUN_SCRIPTLET),
        ("pre", _PRE_SCRIPTLET),
        ("preun", _PREUN_SCRIPTLET),
        ("posttrans", _POSTTRANS_SCRIPTLET),
    ]
]

############################################################################
# versionfile for testing
############################################################################

_VERSION = "1.1.1"

_RELEASE = "2222"

genrule(
    name = "version_file",
    outs = ["version"],
    cmd = "echo '{}' > $@".format(_VERSION),
)

genrule(
    name = "release_file",
    outs = ["release"],
    cmd = "echo '{}' > $@".format(_RELEASE),
)

############################################################################
# Test RPMs
############################################################################

pkg_rpm(
    name = "test_rpm",
    srcs = [
        ":test_pfg",
    ],
    architecture = "noarch",
    conflicts = ["not-a-test"],
    description = """pkg_rpm test rpm description""",
    license = "Apache 2.0",
    post_scriptlet = _POST_SCRIPTLET,
    posttrans_scriptlet = _POSTTRANS_SCRIPTLET,
    postun_scriptlet = _POSTUN_SCRIPTLET,
    pre_scriptlet = _PRE_SCRIPTLET,
    preun_scriptlet = _PREUN_SCRIPTLET,
    provides = ["test"],
    release = _RELEASE,
    requires = ["test-lib > 1.0"],
    requires_contextual = {"preun": ["bash"]},
    spec_template = "template-test.spec.tpl",
    summary = "pkg_rpm test rpm summary",
    version = _VERSION,
)

# Just like the above one, except the compression is changed.
pkg_rpm(
    name = "test_rpm_bzip2",
    srcs = [
        ":test_pfg",
    ],
    architecture = "noarch",
    binary_payload_compression = "w2.bzdio",
    conflicts = ["not-a-test"],
    description = """pkg_rpm test rpm description""",
    license = "Apache 2.0",
    post_scriptlet = _POST_SCRIPTLET,
    posttrans_scriptlet = _POSTTRANS_SCRIPTLET,
    postun_scriptlet = _POSTUN_SCRIPTLET,
    pre_scriptlet = _PRE_SCRIPTLET,
    preun_scriptlet = _PREUN_SCRIPTLET,
    provides = ["test"],
    release = _RELEASE,
    requires = ["test-lib > 1.0"],
    requires_contextual = {"preun": ["bash"]},
    spec_template = "template-test.spec.tpl",
    summary = "pkg_rpm test rpm summary",
    version = _VERSION,
)

# Like the first one, except `srcs` is now passed in without using a
# pkg_filegroup.
pkg_rpm(
    name = "test_rpm_direct",
    srcs = [
        ":ars_pf",
        ":config_file",
        ":test_links",
        ":var_log_foo",
    ],
    architecture = "noarch",
    conflicts = ["not-a-test"],
    description = """pkg_rpm test rpm description""",
    license = "Apache 2.0",
    post_scriptlet = _POST_SCRIPTLET,
    posttrans_scriptlet = _POSTTRANS_SCRIPTLET,
    postun_scriptlet = _POSTUN_SCRIPTLET,
    pre_scriptlet = _PRE_SCRIPTLET,
    preun_scriptlet = _PREUN_SCRIPTLET,
    provides = ["test"],
    release = _RELEASE,
    requires = ["test-lib > 1.0"],
    requires_contextual = {"preun": ["bash"]},
    spec_template = "template-test.spec.tpl",
    summary = "pkg_rpm test rpm summary",
    version = _VERSION,
)

# Like the first one, except we use files for scriptlets
pkg_rpm(
    name = "test_rpm_scriptlets_files",
    srcs = [
        ":test_pfg",
    ],
    architecture = "noarch",
    conflicts = ["not-a-test"],
    description = """pkg_rpm test rpm description""",
    license = "Apache 2.0",
    post_scriptlet_file = ":post",
    posttrans_scriptlet_file = ":posttrans",
    postun_scriptlet_file = ":postun",
    pre_scriptlet_file = ":pre",
    preun_scriptlet_file = ":preun",
    provides = ["test"],
    release = "2222",
    requires = ["test-lib > 1.0"],
    requires_contextual = {"preun": ["bash"]},
    spec_template = "template-test.spec.tpl",
    summary = "pkg_rpm test rpm summary",
    version = _VERSION,
)

# Like the first one, except we use release and version files
pkg_rpm(
    name = "test_rpm_release_version_files",
    srcs = [
        ":test_pfg",
    ],
    architecture = "noarch",
    conflicts = ["not-a-test"],
    description = """pkg_rpm test rpm description""",
    epoch = "1",
    license = "Apache 2.0",
    post_scriptlet_file = ":post",
    posttrans_scriptlet_file = ":posttrans",
    postun_scriptlet_file = ":postun",
    pre_scriptlet_file = ":pre",
    preun_scriptlet_file = ":preun",
    provides = ["test"],
    release_file = ":release_file",
    requires = ["test-lib > 1.0"],
    requires_contextual = {"preun": ["bash"]},
    spec_template = "template-test.spec.tpl",
    summary = "pkg_rpm test rpm summary",
    version_file = ":version_file",
)

# Like the first one, except we set an epoch
pkg_rpm(
    name = "test_rpm_epoch",
    srcs = [
        ":test_pfg",
    ],
    architecture = "noarch",
    conflicts = ["not-a-test"],
    description = """pkg_rpm test rpm description""",
    epoch = "1",
    license = "Apache 2.0",
    post_scriptlet = _POST_SCRIPTLET,
    posttrans_scriptlet = _POSTTRANS_SCRIPTLET,
    postun_scriptlet = _POSTUN_SCRIPTLET,
    pre_scriptlet = _PRE_SCRIPTLET,
    preun_scriptlet = _PREUN_SCRIPTLET,
    provides = ["test"],
    release = _RELEASE,
    requires = ["test-lib > 1.0"],
    requires_contextual = {"preun": ["bash"]},
    spec_template = "template-test.spec.tpl",
    summary = "pkg_rpm test rpm summary",
    version = _VERSION,
)

############################################################################
# Test RPM metadata -- used to verify RPM contents in tests
############################################################################

# Emit a CSV file providing a manifest providing the expected RPM contents
genrule(
    name = "test_rpm_manifest",
    srcs = [
        ":ars",
        ":config_file",
    ],
    outs = ["manifest.csv"],
    # Keep the header (the first line echo'd below) in sync with
    # rpm_queryformat_fieldnames in pkg_rpm_basic_test.py
    cmd = """
    echo 'path,digest,user,group,mode,fflags,symlink' > $@
    for f in $(locations :ars); do
        # Destination path
        (
            echo -n /test/$$(basename $$f),
            # Hash
            $(execpath //tests/util:md5) $$f | tr '\\n' ,
            # User,Group,Mode,Fflags (fflags not provided)
            echo -n 'root,root,100755,'
            # Symlink destination (not provided)
            echo ,
        ) >> $@
    done
    # Config file
    for f in $(location :config_file); do
        (
            echo -n /$$(basename $$f),
            $(execpath //tests/util:md5) $$f | tr '\\n' ,
            # User,Group,Mode,Fflags (fflags "cmn" = config + missingok + noreplace)
            echo -n 'root,root,100644,cmn'
            # Symlink destination (not provided)
            echo ,
        ) >> $@
    done
    # Directory (has no hash)
    (
        echo -n /var/log/foo,
        # No hash (beginning), fflags (end), or symlink destination (end)
        echo ,root,root,40755,,
    ) >> $@

    # Symlink (has no hash)
    (
        echo -n /usr/bin/link-name,
        # No hash (beginning), or fflags (second-to-last)
        echo ,root,root,120777,,/usr/bin/link-target
    ) >> $@
    """,
    tools = ["//tests/util:md5"],
)

genrule(
    name = "test_rpm_metadata",
    srcs = [],
    outs = [
        "conflicts.csv",
        "provides.csv",
        "requires.csv",
    ],
    # In the below, we don't use the "," separator for everything, because the
    # query tags used to get the associated dependency types
    # (e.g. %{REQUIREFLAGS:deptype}) itself uses commas.  This makes it so the test
    # doesn't have to rely on knowing the number of fields in each CSV file.
    cmd = """
    (
        echo 'capability:sense'
        echo 'not-a-test:manual'
    ) > $(RULEDIR)/conflicts.csv
    (
        # NOTE: excludes the "self-require" (we did nothing special to make it
        # appear)

        echo 'capability:sense'
        echo 'test:manual'
        echo 'config(test_rpm) = 1.1.1-2222:config'
    ) > $(RULEDIR)/provides.csv
    (
        # NOTE: excludes 'rpmlib' requires that may be version-dependent
        echo 'capability:sense'
        # Common, automatically generated
        echo '/bin/sh:interp,posttrans'
        echo '/bin/sh:pre,interp'
        echo '/bin/sh:post,interp'
        echo '/bin/sh:preun,interp'
        echo '/bin/sh:postun,interp'
        # Hand-specified, specific dependencies
        echo 'bash:preun'
        # Hand-specified
        echo 'test-lib > 1.0:manual'
        echo 'config(test_rpm) = 1.1.1-2222:config'
    ) > $(RULEDIR)/requires.csv
    """,
)

# One cannot simply pass the output of pkg_rpm as runfiles content (#161).  This
# seems to be the easiest way around this problem.
sh_library(
    name = "pkg_rpm_basic_test_data",
    testonly = True,
    srcs = [
        ":test_rpm",
        ":test_rpm_bzip2",
        ":test_rpm_direct",
        ":test_rpm_epoch",
        ":test_rpm_manifest",
        ":test_rpm_metadata",
        ":test_rpm_release_version_files",
        ":test_rpm_scriptlets_files",
    ],
)

############################################################################
# Confirm that the %dir tag is being applied properly (#473)
############################################################################

pkg_mkdirs(
    name = "dirtest_dirs",
    attributes = pkg_attributes(mode = "0755"),
    dirs = [
        "dir",
    ],
)

pkg_files(
    name = "dirtest_file",
    srcs = [
        ":config_empty",
    ],
    attributes = pkg_attributes(mode = "0644"),
    prefix = "dir",
)

pkg_rpm(
    name = "test_rpm_dirs",
    srcs = [
        # Do not sort.  Order important for testing.
        ":dirtest_file",
        ":dirtest_dirs",
    ],
    architecture = "noarch",
    description = """pkg_rpm test rpm description""",
    license = "Apache 2.0",
    release = "2222",
    spec_template = "template-test.spec.tpl",
    summary = "pkg_rpm test rpm summary",
    version = "1.1.1",
)

genrule(
    name = "test_rpm_dirs_contents",
    srcs = [":test_rpm_dirs"],
    outs = [":test_rpm_dirs_contents.txt"],
    cmd = """
    # pkg_rpm emits two outputs
    RPMS=($(SRCS))
    rpm -qp --queryformat '[%{FILEMODES:perms} %{FILENAMES}\n]' $${RPMS[0]} > $@
    """,
)

diff_test(
    name = "test_rpm_dirs_contents_golden_test",
    file1 = ":test_rpm_dirs_contents",
    file2 = "test_rpm_dirs_contents.txt.golden",
)

############################################################################
# pkg_sub_rpm tests
############################################################################
genrule(
    name = "test_sub_rpm_file_input",
    outs = ["test_sub_rpm_file_input.txt"],
    cmd = """
    echo "test subrpm data" > $@
    """,
)

pkg_files(
    name = "test_sub_rpm_files",
    srcs = [":test_sub_rpm_file_input"],
)

pkg_sub_rpm(
    name = "sub_rpm",
    package_name = "test_sub_rpm",
    srcs = [
        ":test_sub_rpm_files",
    ],
    description = "Test subrpm description",
    summary = "Test subrpm",
)

pkg_sub_rpm(
    name = "sub_rpm2",
    package_name = "test_sub_rpm2",
    srcs = [
        ":test_sub_rpm_files",
    ],
    description = "Test subrpm2 description",
    post_scriptlet = "echo post",
    postun_scriptlet = "echo postun",
    summary = "Test subrpm2",
)

genrule(
    name = "test_sub_rpm_main_file_input",
    outs = ["test_sub_rpm_main_file_input.txt"],
    cmd = """
    echo "test main rpm data" > $@
    """,
)

pkg_files(
    name = "test_sub_rpm_main_files",
    srcs = [":test_sub_rpm_main_file_input"],
)

pkg_rpm(
    name = "test_sub_rpm_main",
    srcs = [
        ":test_sub_rpm_main_files",
    ],
    architecture = "noarch",
    description = "This is a package description.",
    license = "Apache License, v2.0",
    release = "0",
    subrpms = [
        ":sub_rpm",
        ":sub_rpm2",
    ],
    summary = "rules_pkg example RPM",
    version = "1",
)

genrule(
    name = "test_sub_rpm_contents",
    srcs = [":test_sub_rpm_main"],
    outs = [":test_sub_rpm_contents.txt"],
    cmd = """
    # pkg_rpm emits two outputs
    RPMS=($(SRCS))
    echo "===== main RPM =====" > $@
    rpm -qpi --scripts --list $${RPMS[0]} | \
        grep -v 'Build Date' | grep -v 'Build Host' | grep -v 'Relocations' >> $@
    echo "===== sub RPM ======" >> $@
    rpm -qpi --scripts --list $${RPMS[1]} | \
        grep -v 'Build Date' | grep -v 'Build Host' | grep -v 'Relocations' >> $@
    echo "===== sub RPM ======" >> $@
    rpm -qpi --scripts --list $${RPMS[2]} | \
        grep -v 'Build Date' | grep -v 'Build Host' | grep -v 'Relocations' >> $@
    """,
)

diff_test(
    name = "test_golden_sub_rpm_contents",
    file1 = ":test_sub_rpm_contents",
    file2 = "test_sub_rpm_contents.txt.golden",
)

############################################################################
# debuginfo tests
############################################################################
cc_binary(
    name = "test_debuginfo",
    srcs = [
        "test.c",
    ],
    copts = ["-g"],
)

pkg_files(
    name = "test_debuginfo_rpm_files",
    srcs = [
        ":test_debuginfo",
    ],
)

pkg_rpm(
    name = "test_debuginfo_rpm",
    srcs = [
        ":test_debuginfo_rpm_files",
    ],
    debuginfo = True,
    description = "Description",
    license = "Some license",
    release = "0",
    summary = "Summary",
    version = "1",
)

genrule(
    name = "test_debuginfo_rpm_contents",
    srcs = [":test_debuginfo_rpm"],
    outs = [":test_debuginfo_rpm_contents.txt"],
    cmd = """
    # pkg_rpm emits two outputs
    RPMS=($(SRCS))
    echo "===== main RPM =====" > $@
    rpm -qpi --list $${RPMS[0]} | \
        grep -v 'Build Date' | grep -v 'Build Host' | grep -v 'Relocations' | grep -v 'Architecture' | grep -v 'Size' | grep -v '.build-id' >> $@
    echo "===== sub RPM ======" >> $@
    rpm -qpi --list $${RPMS[1]} | \
        grep -v 'Build Date' | grep -v 'Build Host' | grep -v 'Relocations' | grep -v 'Architecture' | grep -v 'Size' | grep -v '.build-id' >> $@
    """,
)

diff_test(
    name = "test_golden_debuginfo_rpm_contents",
    file1 = ":test_debuginfo_rpm_contents",
    file2 = "test_debuginfo_rpm_contents.txt.golden",
)

############################################################################
# Common tests
############################################################################

py_library(
    name = "rpm_util",
    srcs = ["rpm_util.py"],
    imports = ["../.."],
    visibility = [":__subpackages__"],
)

py_test(
    name = "make_rpm_test",
    srcs = ["make_rpm_test.py"],
    python_version = "PY3",
    srcs_version = "PY3",
    deps = [
        "//pkg:make_rpm_lib",
    ],
)

# RPM content verification tests
py_test(
    name = "pkg_rpm_basic_test",
    srcs = ["pkg_rpm_basic_test.py"],
    data = [":pkg_rpm_basic_test_data"],
    python_version = "PY3",
    tags = [
        "no_windows",  # Windows doesn't have rpm(8) or rpmbuild(8)
    ],
    deps = [
        ":rpm_util",
        "@rules_python//python/runfiles",
    ],
)

# Smoke test for defaults
pkg_rpm(
    name = "test_rpm_default_template",
    testonly = True,
    srcs = [
        ":test_pfg",
    ],
    architecture = "noarch",
    description = """pkg_rpm test rpm description""",
    license = "Apache 2.0",
    release = "2222",
    summary = "pkg_rpm test rpm summary",
    version = "1.1.1",
)

build_test(
    name = "pkg_rpm_smoke",
    targets = [":test_rpm_default_template"],
)
