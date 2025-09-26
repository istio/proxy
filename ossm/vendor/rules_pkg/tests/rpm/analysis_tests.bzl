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

"""Tests for RPM generation analysis"""

load("@bazel_skylib//lib:unittest.bzl", "analysistest", "asserts")
load(
    "//pkg:mappings.bzl",
    "pkg_filegroup",
    "pkg_files",
    "pkg_mkdirs",
    "pkg_mklink",
)
load("//pkg:providers.bzl", "PackageVariablesInfo")
load("//pkg:rpm.bzl", "pkg_rpm")
load("//tests/util:defs.bzl", "generic_base_case_test", "generic_negative_test")

def _declare_pkg_rpm(name, srcs_ungrouped, tags = None, **kwargs):
    pfg_name = "{}_pfg".format(name)
    _tags = tags or ["manual"]

    pkg_filegroup(
        name = pfg_name,
        srcs = srcs_ungrouped,
        tags = _tags,
    )

    pkg_rpm(
        name = name,
        srcs = [":" + pfg_name],
        version = "1.0",
        release = "1",
        architecture = "noarch",
        license = "N/A",
        summary = "A test",
        description = "very much a test",
        tags = _tags,
        **kwargs
    )

def _declare_conflicts_test(name, srcs):
    rpm_name = name + "_rpm"
    _declare_pkg_rpm(
        name = rpm_name,
        srcs_ungrouped = srcs,
        tags = ["manual"],
    )

    generic_negative_test(
        name = name,
        target_under_test = ":" + rpm_name,
    )

############################################################
# Begin tests.  Check that the conflict detection system works.
############################################################

def _test_conflicting_inputs(name):
    # The test here is to confirm if pkg_rpm rejects conflicting inputs
    #
    # The structure of the code is such that it's only necessary to test any one
    # packaged item conflicts with all others; order is irrelevant.
    #
    # So, we test how everything would conflict with a "file" entry
    pkg_files(
        name = "{}_file_base".format(name),
        srcs = ["foo"],
        tags = ["manual"],
    )

    _declare_pkg_rpm(
        name = name + "_base",
        srcs_ungrouped = [":{}_file_base".format(name)],
    )

    generic_base_case_test(
        name = name + "_base_case_passes_analysis",
        target_under_test = ":" + name + "_base",
    )

    ##################################################
    # file vs conflicting file
    ##################################################

    pkg_files(
        name = "{}_file_conflict".format(name),
        srcs = ["foo"],
        tags = ["manual"],
    )

    _declare_conflicts_test(
        name = name + "_conflict_with_file",
        srcs = [
            ":{}_file_base".format(name),
            ":{}_file_conflict".format(name),
        ],
    )

    ##################################################
    # file vs conflicting dir
    ##################################################

    pkg_mkdirs(
        name = "{}_dir_conflict".format(name),
        dirs = ["foo"],
        tags = ["manual"],
    )

    _declare_conflicts_test(
        name = name + "_conflict_with_dir",
        srcs = [
            ":{}_file_base".format(name),
            ":{}_dir_conflict".format(name),
        ],
    )

    ##################################################
    # file vs conflicting symbolic link
    ##################################################

    pkg_mklink(
        name = "{}_symlink_conflict".format(name),
        link_name = "foo",
        target = "bar",
        tags = ["manual"],
    )

    _declare_conflicts_test(
        name = name + "_conflict_with_symlink",
        srcs = [
            ":{}_file_base".format(name),
            ":{}_symlink_conflict".format(name),
        ],
    )

    native.test_suite(
        name = name,
        tests = [
            ":{}_{}".format(name, test_name)
            for test_name in [
                "base_case_passes_analysis",
                "conflict_with_file",
                "conflict_with_dir",
                "conflict_with_symlink",
            ]
        ],
    )

############################################################
# Verify that rules produce expected named outputs
############################################################

#### Setup; helpers

def _package_naming_test_impl(ctx):
    env = analysistest.begin(ctx)
    target_under_test = analysistest.target_under_test(env)

    ogi = target_under_test[OutputGroupInfo]

    out_file = ogi.out.to_list()[0]
    rpm_file = ogi.rpm.to_list()[0]
    changes_file = ogi.changes.to_list()[0] if ogi.changes else None
    asserts.equals(
        env,
        rpm_file.basename,
        ctx.attr.expected_name,
        "OutputGroupInfo rpm name does not match expected value.",
    )

    # Try to find the expected files in the DefaultInfo.
    out_file_found = False
    rpm_file_found = False
    changes_file_found = False
    default_name_found = False

    for f in target_under_test[DefaultInfo].files.to_list():
        if f == out_file:
            out_file_found = True
        if f == rpm_file:
            rpm_file_found = True
        if f == changes_file:
            changes_file_found = True
        if f.basename == ctx.attr.expected_name:
            default_name_found = True

    asserts.true(
        env,
        rpm_file_found,
        "rpm component of OutputGroupInfo '{}' is not in DefaultInfo".format(rpm_file),
    )
    asserts.false(
        env,
        out_file_found,
        "out component of OutputGroupInfo '{}' is not in DefaultInfo".format(out_file),
    )
    asserts.false(
        env,
        changes_file_found,
        "changes component of OutputGroupInfo '{}' is not in DefaultInfo".format(changes_file),
    )
    asserts.true(
        env,
        default_name_found,
        "Expected package file with default name '{}' is not in DefaultInfo".format(ctx.attr.expected_name),
    )

    return analysistest.end(env)

package_naming_test = analysistest.make(
    _package_naming_test_impl,
    attrs = {
        "expected_name": attr.string(),
    },
)

# Dummy substitution set, used in below test cases
def _dummy_pkg_variables_impl(_):
    return [
        PackageVariablesInfo(
            values = {
                "FOO": "foo",
                "BAR": "bar",
            },
        ),
    ]

dummy_pkg_variables = rule(
    implementation = _dummy_pkg_variables_impl,
    attrs = {},
)

#### Tests start here

def _test_naming(name):
    # Test whether name templating via PackageVariablesInfo functions as expected, and ensure that
    # outputs are passed through to OutputGroupInfo.
    pkg_files(
        name = "{}_file_base".format(name),
        srcs = ["foo"],
        tags = ["manual"],
    )

    _declare_pkg_rpm(
        name = name + "_no_extra_rpm",
        srcs_ungrouped = [":{}_file_base".format(name)],
    )

    # Default "full" name defaults to the "NVR.A" format.
    package_naming_test(
        name = name + "_no_extra",
        target_under_test = ":" + name + "_no_extra_rpm",
        expected_name = name + "_no_extra_rpm-1.0-1.noarch.rpm",
    )

    ##################################################
    # With pkg_variables
    ##################################################

    dummy_pkg_variables(
        name = name + "_pkg_variables",
    )

    _declare_pkg_rpm(
        name = name + "_with_different_name_rpm",
        srcs_ungrouped = [":{}_file_base".format(name)],
        package_variables = ":{}_pkg_variables".format(name),
        package_file_name = name + "-{FOO}-{BAR}.rpm",
    )

    # Default "full" name defaults to the "NVR.A" format.  Set it to something
    # super arbitrary.
    package_naming_test(
        name = name + "_with_different_name",
        target_under_test = ":" + name + "_with_different_name_rpm",
        expected_name = name + "-foo-bar.rpm",
    )

    ##################################################
    # Test suite declaration
    ##################################################

    native.test_suite(
        name = name,
        tests = [
            ":{}_{}".format(name, test_name)
            for test_name in [
                "no_extra",
                "with_different_name",
            ]
        ],
    )

def analysis_tests(name):
    # Need to test:
    #
    # - Mutual exclusivity of certain options (low priority)
    #
    _test_conflicting_inputs(name = name + "_conflicting_inputs")
    _test_naming(name = name + "_naming")
    native.test_suite(
        name = name,
        tests = [
            name + "_conflicting_inputs",
            name + "_naming",
        ],
    )
