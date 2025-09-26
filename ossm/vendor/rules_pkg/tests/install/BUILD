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
load("//pkg:install.bzl", "pkg_install")
load("//pkg:mappings.bzl", "pkg_attributes", "pkg_files", "pkg_mkdirs")
load("//tests/util:defs.bzl", "directory", "fake_artifact")

package(default_applicable_licenses = ["//:license"])

py_test(
    name = "install_test",
    srcs = ["test.py"],
    args = ["-v"],
    data = [
        ":test_installer",
    ],
    imports = ["../.."],
    main = "test.py",
    tags = [
        # TODO(nacl): investigate this.  See also
        # https://buildkite.com/bazel/rules-pkg/builds/961#992dbe09-7e4b-4e34-91b6-491120476ed3
        "no-windows",
        # TODO(nacl): consider enabling this for testing on Linux.
        #"requires-fakeroot"
    ],
    deps = [
        "//pkg/private:manifest",
        "@rules_python//python/runfiles",
    ],
)

pkg_install(
    name = "test_installer",
    srcs = [
        ":artifact-in-owned-dir",
        ":artifact-in-unowned-dir",
        ":dirs",
        ":generate_tree_pkg_files",
    ],
)

fake_artifact(
    name = "artifact",
)

pkg_files(
    name = "artifact-in-owned-dir",
    srcs = ["artifact"],
    attributes = pkg_attributes(mode = "0700"),
    prefix = "owned-dir",
)

pkg_files(
    name = "artifact-in-unowned-dir",
    srcs = ["artifact"],
    attributes = pkg_attributes(mode = "0755"),
    prefix = "unowned-dir",
)

pkg_mkdirs(
    name = "dirs",
    dirs = [
        "empty-owned-dir",
        "owned-dir",
    ],
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

pkg_files(
    name = "generate_tree_pkg_files",
    srcs = [":generate_tree"],
    attributes = pkg_attributes(
        mode = "640",
    ),
)
