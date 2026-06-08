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

load("@rules_python//python:defs.bzl", "py_library")
load(
    "//pkg:mappings.bzl",
    "pkg_attributes",
    "pkg_files",
    "pkg_mkdirs",
    "strip_prefix",
)
load("//tests/util:defs.bzl", "directory", "link_tree", "write_content_manifest")
load(":mappings_external_repo_test.bzl", "mappings_external_repo_analysis_tests")
load(
    ":mappings_test.bzl",
    "manifest_golden_test",
    "mappings_analysis_tests",
    "mappings_unit_tests",
)

package(default_applicable_licenses = ["//:license"])

py_library(
    name = "manifest_test_lib",
    srcs = ["manifest_test_lib.py"],
    imports = ["../.."],
    srcs_version = "PY3",
    deps = [
        "@rules_python//python/runfiles",
    ],
)

mappings_unit_tests()

mappings_analysis_tests()

mappings_external_repo_analysis_tests()

################################################################################
# Packaging rule manifest tests
################################################################################

directory(
    name = "treeartifact",
    filenames = [
        "artifact_in_tree1",
        "artifact_in_tree2",
    ],
)

pkg_files(
    name = "directory-with-contents",
    srcs = ["treeartifact"],
    attributes = pkg_attributes(mode = "0644"),
)

pkg_mkdirs(
    name = "dirs",
    attributes = pkg_attributes(
        group = "bar",
        mode = "711",
        user = "foo",
    ),
    dirs = [
        "foodir",
    ],
)

pkg_files(
    name = "files",
    srcs = [
        "mappings_test.bzl",
    ],
)

write_content_manifest(
    name = "all",
    srcs = [
        "BUILD",
        ":directory-with-contents",
        ":dirs",
        ":files",
    ],
)

manifest_golden_test(
    name = "all_test",
    expected = "all.manifest.golden",
    target = "all",
)

#
# Unicode handling tests
#
pkg_files(
    name = "utf8_files",
    srcs = [
        "//tests/testdata/utf8:files",
    ],
    strip_prefix = strip_prefix.from_pkg(),
    visibility = ["//tests:__subpackages__"],
)

write_content_manifest(
    name = "utf8",
    srcs = [
        ":utf8_files",
    ],
)

manifest_golden_test(
    name = "utf8_manifest_test",
    expected = "utf8_manifest.golden",
    target = "utf8",
)

# Test handling of executable + runfiles
write_content_manifest(
    name = "executable_manifest",
    srcs = [
        "mappings_test.bzl",
        "//tests:an_executable",
    ],
    include_runfiles = True,
)

alias(
    name = "executable_test_expected",
    actual = select({
        "@platforms//os:windows": "executable.manifest.windows.golden",
        "//conditions:default": "executable.manifest.golden",
    }),
)

manifest_golden_test(
    name = "executable_test",
    expected = ":executable_test_expected",
    target = "executable_manifest",
)

write_content_manifest(
    name = "glob_for_texts_manifest",
    srcs = ["//tests:glob_for_texts"],
)

manifest_golden_test(
    name = "glob_for_texts_manifest_test",
    expected = "glob_for_texts_manifest.golden",
    target = "glob_for_texts_manifest",
)

link_tree(
    name = "node_modules",
    links = {
        "foo": ".pnpm/foo@1.0.0/node_modules/foo",
        ".pnpm/bar@1.0.0/node_modules/bar": "STORE/bar",
        ".pnpm/bar@1.0.0/node_modules/qar": "../../qar@2.0.0/node_modules/qar",
        ".pnpm/foo@1.0.0/node_modules/foo": "STORE/foo",
        ".pnpm/foo@1.0.0/node_modules/bar": "../../bar@1.0.0/node_modules/bar",
        ".pnpm/foo@1.0.0/node_modules/qar": "../../qar@2.0.0/node_modules/qar",
        ".pnpm/qar@2.0.0/node_modules/qar": "STORE/qar",
    },
    package_dir = "node_modules",
)

write_content_manifest(
    name = "node_modules_manifest",
    srcs = [
        ":node_modules",
    ],
)

manifest_golden_test(
    name = "link_tree_test",
    expected = "node_modules_manifest.golden",
    target = "node_modules_manifest",
)
