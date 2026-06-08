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

load("@bazel_skylib//lib:new_sets.bzl", "sets")
load("@bazel_skylib//lib:unittest.bzl", "analysistest", "asserts", "unittest")
load("@rules_python//python:defs.bzl", "py_test")
load(
    "//pkg:mappings.bzl",
    "REMOVE_BASE_DIRECTORY",
    "pkg_attributes",
    "pkg_filegroup",
    "pkg_files",
    "pkg_mkdirs",
    "pkg_mklink",
    "strip_prefix",
)
load(
    "//pkg:providers.bzl",
    "PackageDirsInfo",
    "PackageFilegroupInfo",
    "PackageFilesInfo",
    "PackageSymlinkInfo",
)
load(
    "//tests/util:defs.bzl",
    "directory",
    "fake_artifact",
    "generic_base_case_test",
    "generic_negative_test",
)

##########
# Helpers
##########

def _flatten(list_of_lists):
    """Transform a list of lists into a single list, preserving relative order."""
    return [item for sublist in list_of_lists for item in sublist]

##########
# pkg_files tests
##########

def _pkg_files_contents_test_impl(ctx):
    env = analysistest.begin(ctx)
    target_under_test = analysistest.target_under_test(env)

    expected_dests = {e: None for e in ctx.attr.expected_dests}
    actual_dests = target_under_test[PackageFilesInfo].dest_src_map.keys()

    for actual in actual_dests:
        asserts.true(
            env,
            actual in expected_dests,
            "actual dest <%s> not in expected expected set: %s" % (actual, ctx.attr.expected_dests),
        )
    for expected in expected_dests:
        asserts.true(
            env,
            expected in actual_dests,
            "expected dest <%s> missing from actual set: %s" % (expected, actual_dests),
        )

    # Simple equality checks for the others, if specified
    if ctx.attr.expected_attributes:
        asserts.equals(
            env,
            json.decode(ctx.attr.expected_attributes),
            target_under_test[PackageFilesInfo].attributes,
            "pkg_files attributes do not match expectations",
        )

    # TODO(nacl): verify DefaultInfo propagation

    return analysistest.end(env)

pkg_files_contents_test = analysistest.make(
    _pkg_files_contents_test_impl,
    attrs = {
        # Other attributes can be tested here, but the most important one is the
        # destinations.
        "expected_dests": attr.string_list(
            mandatory = True,
        ),
        "expected_attributes": attr.string(),
    },
)

def _test_pkg_files_contents():
    # Test stripping when no arguments are provided (same as strip_prefix.files_only())
    pkg_files(
        name = "pf_no_strip_prefix_g",
        srcs = ["testdata/hello.txt"],
        attributes = pkg_attributes(
            mode = "0755",
            user = "foo",
            group = "bar",
            # kwargs begins here
            foo = "bar",
        ),
        tags = ["manual"],
    )

    pkg_files_contents_test(
        name = "pf_no_strip_prefix",
        target_under_test = ":pf_no_strip_prefix_g",
        expected_dests = ["hello.txt"],
        expected_attributes = pkg_attributes(
            mode = "0755",
            user = "foo",
            group = "bar",
            foo = "bar",
        ),
    )

    # And now, files_only = True
    pkg_files(
        name = "pf_files_only_g",
        srcs = ["testdata/hello.txt"],
        strip_prefix = strip_prefix.files_only(),
        tags = ["manual"],
    )

    pkg_files_contents_test(
        name = "pf_files_only",
        target_under_test = ":pf_files_only_g",
        expected_dests = ["hello.txt"],
    )

    # Used in the following tests
    fake_artifact(
        name = "testdata/test_script",
        files = ["testdata/a_script.sh"],
        tags = ["manual"],
    )

    # Test stripping from the package root
    pkg_files(
        name = "pf_from_pkg_g",
        srcs = [
            "testdata/hello.txt",
            ":testdata/test_script",
        ],
        strip_prefix = strip_prefix.from_pkg("testdata/"),
        tags = ["manual"],
    )

    pkg_files_contents_test(
        name = "pf_strip_testdata_from_pkg",
        target_under_test = ":pf_from_pkg_g",
        expected_dests = [
            # Static file
            "hello.txt",
            # The script itself
            "a_script.sh",
            # The generated target output, in this case, a symlink
            "test_script",
        ],
    )

    # Test the stripping from root.
    #
    # In this case, the components to be stripped are taken relative to the root
    # of the package.  Local and generated files should have the same prefix in
    # all cases.

    pkg_files(
        name = "pf_from_root_g",
        srcs = [":testdata/test_script"],
        strip_prefix = strip_prefix.from_root("tests/mappings"),
        tags = ["manual"],
    )

    pkg_files_contents_test(
        name = "pf_strip_prefix_from_root",
        target_under_test = ":pf_from_root_g",
        expected_dests = [
            # The script itself
            "testdata/a_script.sh",
            # The generated target output, in this case, a symlink
            "testdata/test_script",
        ],
    )

    # Test that the default mode (0644) is always set regardless of the other
    # values in "attributes".
    pkg_files(
        name = "pf_attributes_mode_overlay_if_not_provided_g",
        srcs = ["foo"],
        strip_prefix = strip_prefix.from_pkg(),
        attributes = pkg_attributes(
            user = "foo",
            group = "bar",
            foo = "bar",
        ),
        tags = ["manual"],
    )
    pkg_files_contents_test(
        name = "pf_attributes_mode_overlay_if_not_provided",
        target_under_test = ":pf_attributes_mode_overlay_if_not_provided_g",
        expected_dests = ["foo"],
        expected_attributes = pkg_attributes(
            mode = "0644",
            user = "foo",
            group = "bar",
            foo = "bar",
        ),
    )

    # Test that pkg_files rejects cases where two targets resolve to the same
    # destination.
    pkg_files(
        name = "pf_destination_collision_invalid_g",
        srcs = ["foo", "bar/foo"],
        tags = ["manual"],
    )
    generic_negative_test(
        name = "pf_destination_collision_invalid",
        target_under_test = ":pf_destination_collision_invalid_g",
    )

    # Test strip_prefix when it can't complete the strip operation as requested.
    pkg_files(
        name = "pf_strip_prefix_from_package_invalid_g",
        srcs = ["foo/foo", "bar/foo"],
        strip_prefix = strip_prefix.from_pkg("bar"),
        tags = ["manual"],
    )
    generic_negative_test(
        name = "pf_strip_prefix_from_package_invalid",
        target_under_test = ":pf_strip_prefix_from_package_invalid_g",
    )

    # Ditto, except strip from the root.
    pkg_files(
        name = "pf_strip_prefix_from_root_invalid_g",
        srcs = ["foo", "bar"],
        strip_prefix = strip_prefix.from_root("not/the/root"),
        tags = ["manual"],
    )
    generic_negative_test(
        name = "pf_strip_prefix_from_root_invalid",
        target_under_test = ":pf_strip_prefix_from_root_invalid_g",
    )

    # Test include_runfiles.
    pkg_files(
        name = "pf_include_runfiles_g",
        include_runfiles = True,
        srcs = ["//tests:an_executable"],
        tags = ["manual"],
    )

    pkg_files_contents_test(
        name = "pf_include_runfiles",
        target_under_test = ":pf_include_runfiles_g",
        expected_dests = select(
            {
                "@bazel_tools//src/conditions:windows": [
                    "an_executable.exe",
                    "an_executable.exe.runfiles/_repo_mapping",
                    "an_executable.exe.runfiles/_main/tests/foo.cc",
                    "an_executable.exe.runfiles/_main/tests/testdata/hello.txt",
                ],
                "//conditions:default": [
                    "an_executable",
                    "an_executable.runfiles/_repo_mapping",
                    "an_executable.runfiles/_main/tests/foo.cc",
                    "an_executable.runfiles/_main/tests/testdata/hello.txt",
                ],
            },
        ),
    )

def _test_pkg_files_exclusions():
    # Normal filegroup, used in all of the below tests
    #
    # Needs to be here to test the distinction between files and label inputs to
    # "excludes".  This, admittedly, may be unnecessary.
    native.filegroup(
        name = "test_base_fg",
        srcs = [
            "testdata/config",
            "testdata/hello.txt",
        ],
    )

    # Tests to exclude from the case where stripping is done up to filenames
    pkg_files(
        name = "pf_exclude_by_label_strip_all_g",
        srcs = [":test_base_fg"],
        excludes = ["//tests/mappings:testdata/config"],
        tags = ["manual"],
    )
    pkg_files_contents_test(
        name = "pf_exclude_by_label_strip_all",
        target_under_test = ":pf_exclude_by_label_strip_all_g",
        expected_dests = ["hello.txt"],
    )

    pkg_files(
        name = "pf_exclude_by_filename_strip_all_g",
        srcs = [":test_base_fg"],
        excludes = ["testdata/config"],
        tags = ["manual"],
    )
    pkg_files_contents_test(
        name = "pf_exclude_by_filename_strip_all",
        target_under_test = ":pf_exclude_by_filename_strip_all_g",
        expected_dests = ["hello.txt"],
    )

    # Tests to exclude from the case where stripping is done from the package root
    pkg_files(
        name = "pf_exclude_by_label_strip_from_pkg_g",
        srcs = [":test_base_fg"],
        excludes = ["//tests/mappings:testdata/config"],
        strip_prefix = strip_prefix.from_pkg("testdata"),
        tags = ["manual"],
    )
    pkg_files_contents_test(
        name = "pf_exclude_by_label_strip_from_pkg",
        target_under_test = ":pf_exclude_by_label_strip_from_pkg_g",
        expected_dests = ["hello.txt"],
    )

    pkg_files(
        name = "pf_exclude_by_filename_strip_from_pkg_g",
        srcs = [":test_base_fg"],
        excludes = ["testdata/config"],
        strip_prefix = strip_prefix.from_pkg("testdata"),
        tags = ["manual"],
    )
    pkg_files_contents_test(
        name = "pf_exclude_by_filename_strip_from_pkg",
        target_under_test = ":pf_exclude_by_filename_strip_from_pkg_g",
        expected_dests = ["hello.txt"],
    )

    # Tests to exclude from the case where stripping is done from the root
    pkg_files(
        name = "pf_exclude_by_label_strip_from_root_g",
        srcs = [":test_base_fg"],
        excludes = ["//tests/mappings:testdata/config"],
        strip_prefix = strip_prefix.from_root("tests/mappings"),
        tags = ["manual"],
    )
    pkg_files_contents_test(
        name = "pf_exclude_by_label_strip_from_root",
        target_under_test = ":pf_exclude_by_label_strip_from_root_g",
        expected_dests = ["testdata/hello.txt"],
    )

    pkg_files(
        name = "pf_exclude_by_filename_strip_from_root_g",
        srcs = [":test_base_fg"],
        excludes = ["testdata/config"],
        strip_prefix = strip_prefix.from_root("tests/mappings"),
        tags = ["manual"],
    )
    pkg_files_contents_test(
        name = "pf_exclude_by_filename_strip_from_root",
        target_under_test = ":pf_exclude_by_filename_strip_from_root_g",
        expected_dests = ["testdata/hello.txt"],
    )

def _test_pkg_files_rename():
    pkg_files(
        name = "pf_rename_multiple_g",
        srcs = [
            "testdata/hello.txt",
            "testdata/loremipsum.txt",
        ],
        prefix = "usr",
        renames = {
            "testdata/hello.txt": "share/goodbye.txt",
            "testdata/loremipsum.txt": "doc/dolorsitamet.txt",
        },
        tags = ["manual"],
    )
    pkg_files_contents_test(
        name = "pf_rename_multiple",
        target_under_test = ":pf_rename_multiple_g",
        expected_dests = [
            "usr/share/goodbye.txt",
            "usr/doc/dolorsitamet.txt",
        ],
    )

    # Used in the following tests
    fake_artifact(
        name = "test_script_rename",
        files = ["testdata/a_script.sh"],
        tags = ["manual"],
    )

    # test_script_rename produces multiple outputs.  Thus, this test should
    # fail, as pkg_files can't figure out what should actually be mapped to
    # the output destination.
    pkg_files(
        name = "pf_rename_rule_with_multiple_outputs_g",
        srcs = ["test_script_rename"],
        renames = {
            ":test_script_rename": "still_a_script",
        },
        tags = ["manual"],
    )
    generic_negative_test(
        name = "pf_rename_rule_with_multiple_outputs",
        target_under_test = ":pf_rename_rule_with_multiple_outputs_g",
    )

    # Fail because we tried to install a file that wasn't mentioned in the deps
    # list
    pkg_files(
        name = "pf_rename_single_missing_value_g",
        srcs = ["testdata/hello.txt"],
        prefix = "usr",
        renames = {
            "a_script": "an_output_location",
        },
        tags = ["manual"],
    )
    generic_negative_test(
        name = "pf_rename_single_missing_value",
        target_under_test = ":pf_rename_single_missing_value_g",
    )

    # Ditto, except for exclusions
    pkg_files(
        name = "pf_rename_single_excluded_value_g",
        srcs = [
            "testdata/hello.txt",
            "testdata/loremipsum.txt",
        ],
        prefix = "usr",
        excludes = [
            "testdata/hello.txt",
        ],
        renames = {
            "testdata/hello.txt": "share/goodbye.txt",
        },
        tags = ["manual"],
    )
    generic_negative_test(
        name = "pf_rename_single_excluded_value",
        target_under_test = ":pf_rename_single_excluded_value_g",
    )

    # Test whether or not destination collisions are detected after renaming.
    pkg_files(
        name = "pf_rename_destination_collision_g",
        srcs = [
            "foo",
            "bar",
        ],
        renames = {"foo": "bar"},
        tags = ["manual"],
    )
    generic_negative_test(
        name = "pf_rename_destination_collision",
        target_under_test = ":pf_rename_destination_collision_g",
    )

    # Test that we are *not* allowed to rename a file to nothing
    pkg_files(
        name = "pf_file_rename_to_empty_g",
        srcs = ["foo"],
        renames = {"foo": REMOVE_BASE_DIRECTORY},
        tags = ["manual"],
    )
    generic_negative_test(
        name = "pf_file_rename_to_empty",
        target_under_test = ":pf_file_rename_to_empty_g",
    )

    # Test that we are allowed to rename a directory to nothing (to "strip" it)
    directory(
        name = "a_directory",
        filenames = ["a_file"],
        tags = ["manual"],
    )
    pkg_files(
        name = "pf_directory_rename_to_empty_g",
        srcs = [":a_directory"],
        renames = {":a_directory": REMOVE_BASE_DIRECTORY},
        tags = ["manual"],
    )
    generic_base_case_test(
        name = "pf_directory_rename_to_empty",
        target_under_test = ":pf_directory_rename_to_empty_g",
    )

##########
# Test pkg_mkdirs
##########

def _pkg_mkdirs_contents_test_impl(ctx):
    env = analysistest.begin(ctx)
    target_under_test = analysistest.target_under_test(env)

    expected_dirs = sets.make(ctx.attr.expected_dirs)
    actual_dirs = sets.make(target_under_test[PackageDirsInfo].dirs)

    asserts.new_set_equals(env, expected_dirs, actual_dirs, "pkg_mkdirs dirs do not match expectations")

    # Simple equality checks for the others
    if ctx.attr.expected_attributes != None:
        asserts.equals(
            env,
            json.decode(ctx.attr.expected_attributes),
            target_under_test[PackageDirsInfo].attributes,
            "pkg_mkdir attributes do not match expectations",
        )

    return analysistest.end(env)

pkg_mkdirs_contents_test = analysistest.make(
    _pkg_mkdirs_contents_test_impl,
    attrs = {
        "expected_attributes": attr.string(),
        "expected_dirs": attr.string_list(
            mandatory = True,
        ),
    },
)

def _test_pkg_mkdirs():
    # Reasonable base case
    pkg_mkdirs(
        name = "pkg_mkdirs_base_g",
        dirs = ["foo/bar", "baz"],
        attributes = pkg_attributes(
            mode = "0711",
            user = "root",
            group = "sudo",
        ),
        tags = ["manual"],
    )
    pkg_mkdirs_contents_test(
        name = "pkg_mkdirs_base",
        target_under_test = ":pkg_mkdirs_base_g",
        expected_dirs = ["foo/bar", "baz"],
        expected_attributes = pkg_attributes(
            mode = "0711",
            user = "root",
            group = "sudo",
        ),
    )

    # Test that the default mode (0755) is always set regardless of the other
    # values in "attributes".
    pkg_mkdirs(
        name = "pkg_mkdirs_mode_overlay_if_not_provided_g",
        dirs = ["foo"],
        attributes = pkg_attributes(
            user = "root",
            group = "sudo",
        ),
        tags = ["manual"],
    )
    pkg_mkdirs_contents_test(
        name = "pkg_mkdirs_mode_overlay_if_not_provided",
        target_under_test = ":pkg_mkdirs_mode_overlay_if_not_provided_g",
        expected_dirs = ["foo"],
        expected_attributes = pkg_attributes(
            mode = "0755",
            user = "root",
            group = "sudo",
        ),
    )

##########
# Test pkg_mklink
##########
def _pkg_mklink_contents_test_impl(ctx):
    env = analysistest.begin(ctx)
    target_under_test = analysistest.target_under_test(env)

    asserts.equals(
        env,
        ctx.attr.expected_target,
        target_under_test[PackageSymlinkInfo].target,
        "pkg_mklink target does not match expectations",
    )

    asserts.equals(
        env,
        ctx.attr.expected_link_name,
        target_under_test[PackageSymlinkInfo].destination,
        "pkg_mklink destination does not match expectations",
    )

    # Simple equality checks for the others, if specified
    if ctx.attr.expected_attributes:
        asserts.equals(
            env,
            json.decode(ctx.attr.expected_attributes),
            target_under_test[PackageSymlinkInfo].attributes,
            "pkg_mklink attributes do not match expectations",
        )

    return analysistest.end(env)

pkg_mklink_contents_test = analysistest.make(
    _pkg_mklink_contents_test_impl,
    attrs = {
        "expected_attributes": attr.string(),
        "expected_link_name": attr.string(mandatory = True),
        "expected_target": attr.string(mandatory = True),
    },
)

def _test_pkg_mklink():
    pkg_mklink(
        name = "pkg_mklink_base_g",
        link_name = "foo",
        target = "bar",
        tags = ["manual"],
        attributes = pkg_attributes(mode = "0111"),
    )

    pkg_mklink_contents_test(
        name = "pkg_mklink_base",
        target_under_test = ":pkg_mklink_base_g",
        expected_link_name = "foo",
        expected_target = "bar",
        expected_attributes = pkg_attributes(mode = "0111"),
    )

    # Test that the default mode (0755) is always set regardless of the other
    # values in "attributes".
    pkg_mklink(
        name = "pkg_mklink_mode_overlay_if_not_provided_g",
        link_name = "foo",
        target = "bar",
        attributes = pkg_attributes(
            user = "root",
            group = "sudo",
        ),
        tags = ["manual"],
    )
    pkg_mklink_contents_test(
        name = "pkg_mklink_mode_overlay_if_not_provided",
        target_under_test = ":pkg_mklink_mode_overlay_if_not_provided_g",
        expected_link_name = "foo",
        expected_target = "bar",
        expected_attributes = pkg_attributes(
            mode = "0777",
            user = "root",
            group = "sudo",
        ),
    )

############################################################
# Test pkg_filegroup
############################################################
def _pkg_filegroup_contents_test_impl(ctx):
    env = analysistest.begin(ctx)
    target_under_test = analysistest.target_under_test(env)

    asserts.equals(
        env,
        [t[PackageFilesInfo] for t in ctx.attr.expected_pkg_files],
        [info for info, _ in target_under_test[PackageFilegroupInfo].pkg_files],
        "pkg_filegroup file list does not match expectations",
    )
    if ctx.attr.verify_origins:
        asserts.equals(
            env,
            [t.label for t in ctx.attr.expected_pkg_files],
            [origin for _, origin in target_under_test[PackageFilegroupInfo].pkg_files],
            "pkg_filegroup file origin list does not match expectations",
        )

    asserts.equals(
        env,
        [t[PackageDirsInfo] for t in ctx.attr.expected_pkg_dirs],
        [info for info, _ in target_under_test[PackageFilegroupInfo].pkg_dirs],
        "pkg_filegroup directory list does not match expectations",
    )
    if ctx.attr.verify_origins:
        asserts.equals(
            env,
            [t.label for t in ctx.attr.expected_pkg_dirs],
            [origin for _, origin in target_under_test[PackageFilegroupInfo].pkg_dirs],
            "pkg_filegroup directory origin list does not match expectations",
        )

    asserts.equals(
        env,
        [t[PackageSymlinkInfo] for t in ctx.attr.expected_pkg_symlinks],
        [info for info, _ in target_under_test[PackageFilegroupInfo].pkg_symlinks],
        "pkg_filegroup symlink map does not match expectations",
    )
    if ctx.attr.verify_origins:
        asserts.equals(
            env,
            [t.label for t in ctx.attr.expected_pkg_symlinks],
            [origin for _, origin in target_under_test[PackageFilegroupInfo].pkg_symlinks],
            "pkg_filegroup symlink origin list does not match expectations",
        )

    # Verify that the DefaultInfo is propagated properly out of the input
    # pkg_files's -- these are the files that need to be passed along to the
    # packaging rules.
    expected_packaged_files = sorted(_flatten([
        t[DefaultInfo].files.to_list()
        for t in ctx.attr.expected_pkg_files
    ]))
    packaged_files = sorted(target_under_test[DefaultInfo].files.to_list())

    asserts.equals(
        env,
        expected_packaged_files,
        packaged_files,
        "pkg_filegroup does not propagate DefaultInfo from pkg_file inputs",
    )

    return analysistest.end(env)

pkg_filegroup_contents_test = analysistest.make(
    _pkg_filegroup_contents_test_impl,
    attrs = {
        "expected_pkg_files": attr.label_list(
            providers = [PackageFilesInfo],
            default = [],
        ),
        "expected_pkg_dirs": attr.label_list(
            providers = [PackageDirsInfo],
            default = [],
        ),
        "expected_pkg_symlinks": attr.label_list(
            providers = [PackageSymlinkInfo],
            default = [],
        ),
        "verify_origins": attr.bool(
            default = True,
        ),
    },
)

def _test_pkg_filegroup(name):
    pkg_files(
        name = "{}_pkg_files".format(name),
        srcs = ["foo", "bar"],
        prefix = "bin",
        tags = ["manual"],
    )

    pkg_mkdirs(
        name = "{}_pkg_dirs".format(name),
        dirs = ["etc"],
        tags = ["manual"],
    )

    pkg_mklink(
        name = "{}_pkg_symlink".format(name),
        link_name = "dest",
        target = "src",
        tags = ["manual"],
    )

    pkg_filegroup(
        name = "{}_pfg".format(name),
        srcs = [t.format(name) for t in ["{}_pkg_files", "{}_pkg_dirs", "{}_pkg_symlink"]],
        tags = ["manual"],
    )

    pkg_filegroup(
        name = "{}_pfg_nested".format(name),
        srcs = [":{}_pfg".format(name)],
        tags = ["manual"],
    )

    # Base case: confirm that basic data translation is working
    pkg_filegroup_contents_test(
        name = "{}_contents_valid".format(name),
        target_under_test = "{}_pfg".format(name),
        expected_pkg_files = ["{}_pkg_files".format(name)],
        expected_pkg_dirs = ["{}_pkg_dirs".format(name)],
        expected_pkg_symlinks = ["{}_pkg_symlink".format(name)],
    )

    # Base case': ditto for nested `pkg_filegroup`s.
    pkg_filegroup_contents_test(
        name = "{}_nested_contents_valid".format(name),
        target_under_test = "{}_pfg_nested".format(name),
        expected_pkg_files = ["{}_pkg_files".format(name)],
        expected_pkg_dirs = ["{}_pkg_dirs".format(name)],
        expected_pkg_symlinks = ["{}_pkg_symlink".format(name)],
        # See below re: "The origins for everything will be wrong here...".
        verify_origins = False,
    )

    ##################################################

    pkg_files(
        name = "{}_pkg_files_prefixed".format(name),
        srcs = ["foo", "bar"],
        prefix = "prefix/bin",
        tags = ["manual"],
    )

    pkg_mkdirs(
        name = "{}_pkg_dirs_prefixed".format(name),
        dirs = ["prefix/etc"],
        tags = ["manual"],
    )

    pkg_mklink(
        name = "{}_pkg_symlink_prefixed".format(name),
        link_name = "prefix/dest",
        target = "src",
        tags = ["manual"],
    )

    # Test that prefixing works by using the unprefixed mapping rules we
    # initially created, and set a prefix.
    pkg_filegroup(
        name = "{}_prefixed_pfg".format(name),
        srcs = [t.format(name) for t in ["{}_pkg_files", "{}_pkg_dirs", "{}_pkg_symlink"]],
        prefix = "prefix",
        tags = ["manual"],
    )

    # Then test that the results are equivalent to manually specifying the
    # prefix everywhere.
    pkg_filegroup_contents_test(
        name = "{}_contents_prefix_translated".format(name),
        target_under_test = "{}_prefixed_pfg".format(name),
        expected_pkg_files = ["{}_pkg_files_prefixed".format(name)],
        expected_pkg_dirs = ["{}_pkg_dirs_prefixed".format(name)],
        expected_pkg_symlinks = ["{}_pkg_symlink_prefixed".format(name)],
        # The origins for everything will be wrong here, since they're derived
        # from the labels of the inputs to pkg_filegroup.
        #
        # The first test here should be adequate for this purpose.
        verify_origins = False,
    )

    # Now do the same for a nested `pkg_filegroup`.
    pkg_files(
        name = "{}_pkg_files_nested_prefixed".format(name),
        srcs = ["foo", "bar"],
        prefix = "nest/prefix/bin",
        tags = ["manual"],
    )

    pkg_mkdirs(
        name = "{}_pkg_dirs_nested_prefixed".format(name),
        dirs = ["nest/prefix/etc"],
        tags = ["manual"],
    )

    pkg_mklink(
        name = "{}_pkg_symlink_nested_prefixed".format(name),
        link_name = "nest/prefix/dest",
        target = "src",
        tags = ["manual"],
    )

    pkg_filegroup(
        name = "{}_nested_prefixed_pfg".format(name),
        srcs = [":{}_prefixed_pfg".format(name)],
        prefix = "nest",
        tags = ["manual"],
    )

    pkg_filegroup_contents_test(
        name = "{}_contents_nested_prefix_translated".format(name),
        target_under_test = "{}_nested_prefixed_pfg".format(name),
        expected_pkg_files = ["{}_pkg_files_nested_prefixed".format(name)],
        expected_pkg_dirs = ["{}_pkg_dirs_nested_prefixed".format(name)],
        expected_pkg_symlinks = ["{}_pkg_symlink_nested_prefixed".format(name)],
        # See above re: "The origins for everything will be wrong here...".
        verify_origins = False,
    )

    native.test_suite(
        name = name,
        tests = [
            t.format(name)
            for t in [
                "{}_contents_valid",
                "{}_nested_contents_valid",
                "{}_contents_prefix_translated",
                "{}_contents_nested_prefix_translated",
            ]
        ],
    )

##########
# Test strip_prefix pseudo-module
##########

def _strip_prefix_test_impl(ctx):
    env = unittest.begin(ctx)
    asserts.equals(env, ".", strip_prefix.files_only())
    asserts.equals(env, "path", strip_prefix.from_pkg("path"))
    asserts.equals(env, "path", strip_prefix.from_pkg("/path"))
    asserts.equals(env, "/path", strip_prefix.from_root("path"))
    asserts.equals(env, "/path", strip_prefix.from_root("/path"))
    return unittest.end(env)

strip_prefix_test = unittest.make(_strip_prefix_test_impl)

# buildifier: disable=unnamed-macro
def mappings_analysis_tests():
    """Declare mappings.bzl analysis tests"""
    _test_pkg_files_contents()
    _test_pkg_files_exclusions()
    _test_pkg_files_rename()
    _test_pkg_mkdirs()
    _test_pkg_mklink()

    # TODO(nacl) migrate the above to use a scheme the one used here.  At the very
    # least, the test suites should be easy to find/name.
    _test_pkg_filegroup(name = "pfg_tests")

    native.test_suite(
        name = "pkg_files_analysis_tests",
        # We should find a way to get rid of this test list; it would be nice if
        # it could be derived from something else...
        tests = [
            # buildifier: don't sort
            # Simple tests
            ":pf_no_strip_prefix",
            ":pf_files_only",
            ":pf_strip_testdata_from_pkg",
            ":pf_strip_prefix_from_root",
            ":pf_attributes_mode_overlay_if_not_provided",
            # Tests involving excluded files
            ":pf_exclude_by_label_strip_all",
            ":pf_exclude_by_filename_strip_all",
            ":pf_exclude_by_label_strip_from_pkg",
            ":pf_exclude_by_filename_strip_from_pkg",
            ":pf_exclude_by_label_strip_from_root",
            ":pf_exclude_by_filename_strip_from_root",
            # Negative tests
            ":pf_destination_collision_invalid",
            ":pf_strip_prefix_from_package_invalid",
            ":pf_strip_prefix_from_root_invalid",
            #
            # Tests involving file renaming
            ":pf_rename_multiple",
            ":pf_rename_rule_with_multiple_outputs",
            ":pf_rename_single_missing_value",
            ":pf_rename_single_excluded_value",
            # Tests involving pkg_mkdirs
            ":pkg_mkdirs_base",
            ":pkg_mkdirs_mode_overlay_if_not_provided",
            # Tests involving pkg_mklink
            ":pkg_mklink_base",
            ":pkg_mklink_mode_overlay_if_not_provided",
            # Tests involving pkg_filegroup
            ":pfg_tests",
        ],
    )

def mappings_unit_tests():
    unittest.suite(
        "mappings_unit_tests",
        strip_prefix_test,
    )

def _gen_manifest_test_main_impl(ctx):
    ctx.actions.expand_template(
        template = ctx.file._template,
        output = ctx.outputs.out,
        substitutions = {
            "${EXPECTED}": ctx.files.expected[0].short_path,
            "${TARGET}": ctx.files.target[0].short_path,
            "${TEST_NAME}": ctx.attr.test_name,
        },
    )
    return [
        DefaultInfo(files = depset([ctx.outputs.out])),
    ]

_gen_manifest_test_main = rule(
    implementation = _gen_manifest_test_main_impl,
    attrs = {
        "out": attr.output(mandatory = True),
        "expected": attr.label(mandatory = True, allow_single_file = True),
        "target": attr.label(mandatory = True, allow_single_file = True),
        "test_name": attr.string(mandatory = True),
        "_template": attr.label(
            default = Label("//tests/mappings:manifest_test_main.py.tpl"),
            allow_single_file = True,
        ),
    },
)

def manifest_golden_test(name, target, expected):
    """Tests that a content manifest file matches a golden copy.

    This test is used to verify that a generated manifest file matches the
    expected content.

    Args:
      name: name
      target: A target which produces a content manifest with the name
          <target> + ".manifest"
      expected: label of a file containing the expected content.
    """
    _gen_manifest_test_main(
        name = name + "_main",
        out = name + ".py",
        expected = expected,
        target = target + ".manifest",
        test_name = target + "Test",
    )
    py_test(
        name = name,
        srcs = [":" + name + ".py"],
        data = [
            ":" + target,
            ":" + target + ".manifest",
            expected,
        ],
        python_version = "PY3",
        deps = [
            ":manifest_test_lib",
        ],
    )
