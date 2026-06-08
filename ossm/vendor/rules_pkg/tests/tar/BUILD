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
# -*- coding: utf-8 -*-
"""Tests for pkg_tar."""

load("@rules_python//python:defs.bzl", "py_binary", "py_test")

# buildifier: disable=bzl-visibility
load("//pkg:mappings.bzl", "pkg_attributes", "pkg_files", "pkg_mkdirs", "pkg_mklink")
load("//pkg:verify_archive.bzl", "verify_archive_test")

# buildifier: disable=bzl-visibility
load("//pkg/private/tar:tar.bzl", "SUPPORTED_TAR_COMPRESSIONS", "pkg_tar")
load("//tests:my_package_name.bzl", "my_package_naming")
load("//tests/util:defs.bzl", "directory", "fake_artifact", "link_tree")
load(":defs.bzl", "raw_symlinks")

package(
    default_applicable_licenses = ["//:license"],
    default_visibility = ["//visibility:private"],
)

py_test(
    name = "tar_writer_test",
    srcs = [
        "tar_writer_test.py",
    ],
    data = [
        ":compressor",
        ":test_tar_compression",
        ":test_tar_package_dir",
        ":test_tar_package_dir_file",
        "//tests:testdata/tar_test.tar",
        "//tests:testdata/tar_test.tar.bz2",
        "//tests:testdata/tar_test.tar.gz",
        "//tests:testdata/tar_test.tar.xz",
        "//tests:testdata/test_tar_package_dir_file.txt",
    ],
    imports = ["../.."],
    python_version = "PY3",
    srcs_version = "PY3",
    deps = [
        "//pkg/private/tar:tar_writer",
        "@rules_python//python/runfiles",
    ],
)

genrule(
    name = "generate_files",
    outs = [
        "etc/nsswitch.conf",
        "usr/titi",
    ],
    cmd = "for i in $(OUTS); do echo 1 >$$i; done",
)

directory(
    name = "generate_tree",
    contents = "hello there",
    filenames = [
        # buildifier: don't sort
        "b/e",
        "a/a",
        "b/c/d",
        "b/d",
        "a/b/c",
    ],
)

py_binary(
    name = "compressor",
    srcs = ["compressor.py"],
    python_version = "PY3",
    srcs_version = "PY3",
)

pkg_tar(
    name = "test_tar_compression",
    compressor = ":compressor",
    compressor_args = "-a -b -c",
    deps = ["//tests:testdata/tar_test.tar"],
)

pkg_tar(
    name = "test-respect-externally-defined-duplicates",
    allow_duplicates_from_deps = True,
    create_parents = False,
    deps = ["//tests:testdata/duplicate_entries.tar"],
)

#
# Tests for package_file_name
#
my_package_naming(
    name = "my_package_variables",
    label = "some_value",
)

pkg_tar(
    name = "test_tar_naming",
    srcs = [
        ":BUILD",
    ],
    package_file_name = "test_naming_{label}.tar",
    package_variables = ":my_package_variables",
)

pkg_mkdirs(
    name = "mydir",
    dirs = [
        "mydir",
    ],
)

pkg_tar(
    name = "test_tar_package_dir_substitution",
    srcs = [
        ":BUILD",
        ":mydir",
    ],
    package_dir = "level1/{label}/level3",
    package_variables = ":my_package_variables",
)

pkg_mklink(
    name = "mylink",
    link_name = "mylink",
    target = "dangling",
)

pkg_tar(
    name = "test_tar_package_dir",
    srcs = [
        ":etc/nsswitch.conf",
        ":mylink",
    ],
    package_dir = "/my/package",
)

pkg_tar(
    name = "test_tar_package_dir_file",
    srcs = [
        ":etc/nsswitch.conf",
    ],
    package_dir_file = "//tests:testdata/test_tar_package_dir_file.txt",
)

pkg_tar(
    name = "test_tar_out",
    srcs = [
        ":BUILD",
    ],
    out = "out.tar",
)

[pkg_tar(
    name = "test-tar-basic-%s" % ext,
    srcs = [
        ":etc/nsswitch.conf",
        ":usr/titi",
    ],
    extension = "tar.%s" % ext if ext else "tar",
    mode = "0644",
    modes = {"usr/titi": "0755"},
    owner = "42.24",
    ownername = "titi.tata",
    ownernames = {"etc/nsswitch.conf": "tata.titi"},
    owners = {"etc/nsswitch.conf": "24.42"},
    package_dir = "/",
    strip_prefix = ".",
    symlinks = {"usr/bin/java": "/path/to/bin/java"},
) for ext in SUPPORTED_TAR_COMPRESSIONS]

[pkg_tar(
    name = "test-tar-inclusion-%s" % ext,
    deps = [
        ":test-tar-basic-%s" % ext,
        ":test_tar_naming",
    ],
) for ext in SUPPORTED_TAR_COMPRESSIONS]

pkg_tar(
    name = "test-tar-strip_prefix-empty",
    srcs = [
        ":etc/nsswitch.conf",
    ],
    strip_prefix = "",
)

pkg_tar(
    name = "test-tar-strip_prefix-none",
    srcs = [
        ":etc/nsswitch.conf",
    ],
)

pkg_tar(
    name = "test-tar-strip_prefix-etc",
    srcs = [
        ":etc/nsswitch.conf",
    ],
    strip_prefix = "etc",
)

pkg_tar(
    name = "test-tar-strip_prefix-dot",
    srcs = [
        ":etc/nsswitch.conf",
        "@rules_python//python/runfiles",
    ],
    strip_prefix = ".",
)

pkg_tar(
    name = "test-tar-strip_prefix-substring",
    srcs = [
        ":etc/nsswitch.conf",
    ],
    strip_prefix = "et",
)

pkg_tar(
    name = "test-tar-files_dict",
    files = {
        ":etc/nsswitch.conf": "not-etc/mapped-filename.conf",
    },
)

pkg_tar(
    name = "test-tar-empty_files",
    empty_files = [
        "/a",
        "/b",
    ],
    mode = "0o777",
)

pkg_tar(
    name = "test-tar-empty_dirs",
    empty_dirs = [
        "/tmp",
        "/pmt",
    ],
    mode = "0o777",
)

pkg_tar(
    name = "test-tar-mtime",
    srcs = [
        ":etc/nsswitch.conf",
    ],
    mtime = 946684740,  # 1999-12-31, 23:59
    portable_mtime = False,
)

pkg_tar(
    name = "test-tar-long-filename",
    srcs = [
        "//tests:testdata/file_with_a_ridiculously_long_name_consectetur_adipiscing_elit_fusce_laoreet_lorem_neque_sed_pharetra_erat.txt",
    ],
    # TODO(https://github.com/bazelbuild/rules_pkg/issues/462): The old test
    # used to have strip_prefix. But strip_prefix is just plain broken w.r.t.
    # files from other packages. We end up with an internal expected data_path
    # of tests/tar/<strip_prefix>. But the files coming in have the real paths.
    # strip_prefix = "testdata",
)

pkg_tar(
    name = "test-tar-repackaging-long-filename",
    package_dir = "can_i_repackage_a_file_with_a_long_name",
    deps = [
        ":test-tar-long-filename",
    ],
)

verify_archive_test(
    name = "repackaging_long_filename_test",
    max_size = 2,
    must_contain = [
        "can_i_repackage_a_file_with_a_long_name/file_with_a_ridiculously_long_name_consectetur_adipiscing_elit_fusce_laoreet_lorem_neque_sed_pharetra_erat.txt",
    ],
    must_contain_regex = [
        ".*can_i_repackage_a_file_with_a_long_name/",
    ],
    # there is really no need for these cases. I just want to use all the test capabilities.
    must_not_contain = [
        "i_am_not here",
    ],
    must_not_contain_regex = [
        "^five.is.right.out",
    ],
    target = ":test-tar-repackaging-long-filename",
)

pkg_tar(
    name = "test-tar-tree-artifact",
    srcs = [
        ":generate_tree",
        "//tests:loremipsum_txt",
    ],
    package_dir = "a_tree",
)

pkg_files(
    name = "tree_noroot",
    srcs = [
        ":generate_tree",
    ],
    strip_prefix = "generate_tree",
)

pkg_tar(
    name = "test-tar-tree-artifact-noroot",
    srcs = [
        ":tree_noroot",
        "//tests:loremipsum_txt",
    ],
    package_dir = "a_tree",
)

fake_artifact(
    name = "a_program",
    executable = True,
    files = ["//tests:testdata/executable.sh"],
    runfiles = ["BUILD"],
)

pkg_tar(
    name = "test-tar-with-runfiles",
    srcs = [
        ":a_program",
        "//tests:an_executable",
    ],
    include_runfiles = True,
)

verify_archive_test(
    name = "runfiles_test",
    must_contain = [
        "a_program",
        "a_program.runfiles/_main/tests/tar/BUILD",
        "executable.sh",
    ] + select({
        "@platforms//os:windows": [
            "an_executable.exe",
            "an_executable.exe.runfiles/_main/tests/foo.cc",
            "an_executable.exe.runfiles/_main/tests/an_executable.exe",
            "an_executable.exe.runfiles/_main/tests/testdata/hello.txt",
        ],
        "//conditions:default": [
            "an_executable",
            "an_executable.runfiles/_main/tests/foo.cc",
            "an_executable.runfiles/_main/tests/an_executable",
            "an_executable.runfiles/_main/tests/testdata/hello.txt",
        ],
    }),
    target = ":test-tar-with-runfiles",
)

pkg_tar(
    name = "test-tar-remap-runfiles-base-path",
    srcs = [
        ":a_program",
        "//tests:an_executable",
    ],
    include_runfiles = True,
    remap_paths = {
        "/a_program": "/new_name",
        "/an_executable": "/other/different_name",
    },
)

verify_archive_test(
    name = "runfiles_remap_base_path_test",
    must_contain = [
        "new_name",
        "new_name.runfiles/_main/tests/tar/BUILD",
        "executable.sh",
    ] + select({
        "@platforms//os:windows": [
            "other/different_name.exe",
            "other/different_name.exe.runfiles/_main/tests/foo.cc",
            "other/different_name.exe.runfiles/_main/tests/an_executable.exe",
            "other/different_name.exe.runfiles/_main/tests/testdata/hello.txt",
        ],
        "//conditions:default": [
            "other/different_name",
            "other/different_name.runfiles/_main/tests/foo.cc",
            "other/different_name.runfiles/_main/tests/an_executable",
            "other/different_name.runfiles/_main/tests/testdata/hello.txt",
        ],
    }),
    target = ":test-tar-remap-runfiles-base-path",
)

pkg_tar(
    name = "test-tar-remap-runfiles-full-paths",
    srcs = [
        ":a_program",
        "//tests:an_executable",
    ],
    include_runfiles = True,
    remap_paths = {
        # rename the entire runfiles directory
        "/a_program.runfiles/": "/a/program/runfiles/",
        # rename a specific file
        "/an_executable.runfiles/_main/tests/testdata/hello.txt": "/myfiles/hello.txt",
        "/an_executable.exe.runfiles/_main/tests/testdata/hello.txt": "/myfiles/hello.txt",
        # rename a specific subdirectory
        "/an_executable.runfiles/_main/tests/": "/mytests/",
        "/an_executable.exe.runfiles/_main/tests/": "/mytests/",
    },
)

verify_archive_test(
    name = "runfiles_remap_full_paths_test",
    must_contain = [
        "a_program",
        "a/program/runfiles/_main/tests/tar/BUILD",
        "executable.sh",
    ] + select({
        "@platforms//os:windows": [
            "an_executable.exe",
            "mytests/foo.cc",
            "mytests/an_executable.exe",
            "myfiles/hello.txt",
        ],
        "//conditions:default": [
            "an_executable",
            "mytests/foo.cc",
            "mytests/an_executable",
            "myfiles/hello.txt",
        ],
    }),
    target = ":test-tar-remap-runfiles-full-paths",
)

pkg_tar(
    name = "test_tar_leading_dotslash",
    srcs = [
        "//tests:loremipsum_txt",
    ],
    package_dir = ".",
)

py_test(
    name = "pkg_tar_test",
    size = "medium",
    srcs = [
        "pkg_tar_test.py",
    ],
    data = [
        ":test-pkg-tar-from-pkg-files-with-attributes",
        ":test-pkg-tar-with-attributes",
        ":test-remap-paths-tree-artifact",
        ":test-respect-externally-defined-duplicates.tar",
        ":test-tar-compression_level--1",
        ":test-tar-compression_level-3",
        ":test-tar-compression_level-6",
        ":test-tar-compression_level-9",
        ":test-tar-empty_dirs.tar",
        ":test-tar-empty_files.tar",
        ":test-tar-files_dict.tar",
        ":test-tar-long-filename",
        ":test-tar-mtime.tar",
        ":test-tar-preserve_mode-False.tar",
        ":test-tar-preserve_mode-True.tar",
        ":test-tar-preserve_mtime-False.tar",
        ":test-tar-preserve_mtime-True.tar",
        ":test-tar-repackaging-long-filename.tar",
        ":test-tar-strip_prefix-dot.tar",
        ":test-tar-strip_prefix-empty.tar",
        ":test-tar-strip_prefix-etc.tar",
        ":test-tar-strip_prefix-none.tar",
        ":test-tar-strip_prefix-substring.tar",
        ":test-tar-tree-artifact",
        ":test-tar-tree-artifact-noroot",
        ":test-tar-xz-compression_level--1",
        ":test-tar-xz-compression_level-3",
        ":test-tar-xz-compression_level-6",
        ":test-tar-xz-compression_level-9",
        ":test-tree-input-with-strip-prefix",
        ":test_tar_leading_dotslash",
        ":test_tar_package_dir_substitution.tar",
        "//tests:testdata/tar_test.tar",
        "//tests:testdata/tar_test.tar.bz2",
        "//tests:testdata/tar_test.tar.gz",
        "//tests:testdata/tar_test.tar.xz",
        "//tests:testdata/test_tar_package_dir_file.txt",
    ] + [
        ":test-tar-basic-%s" % compression
        for compression in SUPPORTED_TAR_COMPRESSIONS
    ] + [
        ":test-tar-inclusion-%s" % compression
        for compression in SUPPORTED_TAR_COMPRESSIONS
    ],
    imports = ["../.."],
    python_version = "PY3",
    deps = [
        "//pkg/private/tar:tar_writer",
        "@rules_python//python/runfiles",
    ],
)

test_suite(
    name = "windows_tests",
    tags = [
        "-slow",
    ],
    visibility = ["//visibility:private"],
)

test_suite(
    name = "all_windows_tests",
    tests = [":windows_tests"],
)

pkg_tar(
    name = "test-tar-compression-from-extension-targz",
    srcs = [
        "//tests:testdata/loremipsum.txt",
    ],
    extension = ".tar.gz",
)

pkg_files(
    name = "test-pkg-files-with-attributes",
    srcs = [
        "//tests:testdata/loremipsum.txt",
    ],
    attributes = pkg_attributes(
        gid = 1000,
        group = "grp",
        mode = "0440",
        uid = 0,
        user = "person",
    ),
    prefix = "/foo/bar",
)

pkg_tar(
    name = "test-pkg-tar-from-pkg-files-with-attributes",
    srcs = [
        ":test-pkg-files-with-attributes",
    ],
)

pkg_tar(
    name = "test-pkg-tar-with-attributes",
    srcs = [
        "//tests:testdata/loremipsum.txt",
    ],
    owner = "0.1000",
    package_dir = "/foo/bar",
)

pkg_tar(
    name = "test-tar-compression-from-extension-tgz",
    srcs = [
        "//tests:testdata/loremipsum.txt",
    ],
    extension = "tgz",
)

pkg_tar(
    name = "test-tar-compression-from-extension-bz2",
    srcs = [
        "//tests:testdata/loremipsum.txt",
    ],
    extension = ".bz2",
)

py_test(
    name = "is_compressed_test",
    srcs = ["is_compressed_test.py"],
    data = [
        ":test-tar-compression-from-extension-bz2",
        ":test-tar-compression-from-extension-targz",
        ":test-tar-compression-from-extension-tgz",
    ],
    python_version = "PY3",
    srcs_version = "PY3",
    deps = [
        "@rules_python//python/runfiles",
    ],
)

directory(
    name = "generate_tree_with_prefix",
    contents = "hello there",
    filenames = [
        "a/a",
        "a/b",
    ],
    outdir = "tree_prefix_to_strip",
)

pkg_tar(
    name = "test-tree-input-with-strip-prefix",
    srcs = [
        ":generate_tree_with_prefix",
    ],
    strip_prefix = "tree_prefix_to_strip",
)

directory(
    name = "tree_artifact_to_rename",
    contents = "hello there",
    filenames = [
        "a",
        "rename_me/should_not_rename",
    ],
    outdir = "rename_me",
)

pkg_tar(
    name = "test-remap-paths-tree-artifact",
    srcs = [
        ":tree_artifact_to_rename",
    ],
    remap_paths = {
        "/rename_me": "a_new_name",
    },
)

#
# Test with symlinks
#

raw_symlinks(
    name = "raw_symlinks",
    tags = ["REQUIRES_BAZEL_8"],
)

pkg_tar(
    name = "raw_symlinks_tar",
    srcs = [":raw_symlinks"],
    include_runfiles = True,
    tags = ["REQUIRES_BAZEL_8"],
)

verify_archive_test(
    name = "raw_symlinks_test",
    tags = [
        "REQUIRES_BAZEL_8",
        "no_windows",
    ],
    target = ":raw_symlinks_tar",
    # This test checks using relative symlinks, but Bazel always writes absolute
    # symlinks on windows.
    # See https://github.com/bazelbuild/bazel/issues/14224
    target_compatible_with = select({
        "@platforms//os:windows": ["@platforms//:incompatible"],
        "//conditions:default": [],
    }),
    verify_links = {
        "raw_symlinks_link1": "link1",
        "raw_symlinks_link1.runfiles/_main/tests/tar/raw_symlinks_runfile_link": "../runfile_link",
    },
)

link_tree(
    name = "node_modules",
    links = {
        "foo": ".pnpm/foo@1.0.0/node_modules/foo",
        ".pnpm/bar@1.0.0/node_modules/bar": "STORE/bar",
        ".pnpm/foo@1.0.0/node_modules/foo": "STORE/foo",
        ".pnpm/foo@1.0.0/node_modules/bar": "../../bar@1.0.0/node_modules/bar",
    },
    package_dir = "node_modules",
)

directory(
    name = "tree_artifact_with_links",
    contents = "hello there",
    filenames = [
        "foo/hello.txt",
        "foo/bar/baz",
    ],
    # TODO(https://github.com/bazelbuild/rules_pkg/issues/750)
    #links = {
    #    "foo/bar/hello": "../hello.txt",
    #    "foo/bar/alt_baz": "baz",
    #    "foo/alt_baz": "bar/baz",
    #},
)

# This target has symlinks 3 ways.
# - mklinks rules.
# - source files that are symlinks
# - tree artifacts
pkg_tar(
    name = "relative_links_tar",
    srcs = [
        ":node_modules",
        ":tree_artifact_with_links",
        "//tests:file_and_link",
    ],
    symlinks = {
        "tree_artifact/toss_in_another": "/foo",
    },
)

verify_archive_test(
    name = "relative_links_test",
    min_size = 10,
    must_contain = [
        "BUILD",
        "outer_BUILD",
        "node_modules/.pnpm/bar@1.0.0/node_modules/bar",
    ],
    tags = ["nowindows"],
    target = ":relative_links_tar",
    verify_links = {
        "node_modules/.pnpm/foo@1.0.0/node_modules/bar": "../../bar@1.0.0/node_modules/bar",
        "tree_artifact/toss_in_another": "/foo",
    },
)

pkg_tar(
    name = "relative_links_re_tar",
    package_dir = "new/base",
    symlinks = {
        # We expect package_dir to apply to this
        "tree_artifact/toss_in_another": "some_target",
        # But we protect the user from duplicating package_dir in their
        # link. That was the old behavior of symlink.
        "new/base/something/this": "that",
    },
    deps = [
        ":relative_links_tar",
    ],
)

verify_archive_test(
    name = "relative_links_re_test",
    min_size = 10,
    must_contain = [
        "new/base/BUILD",
        "new/base/outer_BUILD",
        "new/base/node_modules/.pnpm/bar@1.0.0/node_modules/bar",
    ],
    must_not_contain = [
        "BUILD",
        "new/base/new/base/something/this",
    ],
    tags = ["nowindows"],
    target = ":relative_links_re_tar",
    verify_links = {
        "new/base/node_modules/.pnpm/foo@1.0.0/node_modules/bar": "../../bar@1.0.0/node_modules/bar",
        "new/base/tree_artifact/toss_in_another": "some_target",
        "new/base/something/this": "that",
    },
)

fake_artifact(
    name = "program_with_dir_runfiles",
    files = ["//tests:testdata/executable.sh"],
    runfiles = [
        ":generate_tree",
    ],
)

pkg_tar(
    name = "program_with_dir_runfiles_tar",
    srcs = [
        ":program_with_dir_runfiles",
    ],
    include_runfiles = True,
)

verify_archive_test(
    name = "program_with_dir_runfiles_test",
    must_contain = [
        "program_with_dir_runfiles.runfiles",
        "program_with_dir_runfiles.runfiles/_main",
        "program_with_dir_runfiles.runfiles/_main/tests",
        "program_with_dir_runfiles.runfiles/_main/tests/tar",
        "program_with_dir_runfiles.runfiles/_main/tests/tar/generate_tree",
        "program_with_dir_runfiles.runfiles/_main/tests/tar/generate_tree/a",
        "program_with_dir_runfiles.runfiles/_main/tests/tar/generate_tree/a/a",
        "program_with_dir_runfiles.runfiles/_main/tests/tar/generate_tree/a/b",
        "program_with_dir_runfiles.runfiles/_main/tests/tar/generate_tree/a/b/c",
        "program_with_dir_runfiles.runfiles/_main/tests/tar/generate_tree/b",
        "program_with_dir_runfiles.runfiles/_main/tests/tar/generate_tree/b/c",
        "program_with_dir_runfiles.runfiles/_main/tests/tar/generate_tree/b/c/d",
        "program_with_dir_runfiles.runfiles/_main/tests/tar/generate_tree/b/d",
        "program_with_dir_runfiles.runfiles/_main/tests/tar/generate_tree/b/e",
        "executable.sh",
        "program_with_dir_runfiles",
    ],
    target = ":program_with_dir_runfiles_tar",
)

[pkg_tar(
    name = "test-tar-compression_level-%s" % compression_level,
    compression_level = compression_level,
    extension = "tgz",
    deps = [
        "//tests:testdata/tar_test.tar",
    ],
) for compression_level in [
    -1,
    3,
    6,
    9,
]]

[pkg_tar(
    name = "test-tar-xz-compression_level-%s" % compression_level,
    compression_level = compression_level,
    extension = "tar.xz",
    deps = [
        "//tests:testdata/tar_xz_test.tar",
    ],
) for compression_level in [
    -1,
    3,
    6,
    9,
]]

[pkg_tar(
    name = "test-tar-preserve_mode-%s" % state,
    srcs = [
        "//tests:testdata/hello.txt",  # rw- r-- r--
    ],
    preserve_mode = state,
) for state in [
    True,
    False,
]]

[pkg_tar(
    name = "test-tar-preserve_mtime-%s" % state,
    srcs = [
        "//tests:testdata/hello.txt",
    ],
    preserve_mtime = state,
) for state in [
    True,
    False,
]]
