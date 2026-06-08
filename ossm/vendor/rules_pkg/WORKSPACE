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

workspace(name = "rules_pkg")

load("@rules_pkg//pkg:deps.bzl", "rules_pkg_dependencies")

rules_pkg_dependencies()

load("@bazel_skylib//:workspace.bzl", "bazel_skylib_workspace")

bazel_skylib_workspace()

### INTERNAL ONLY - lines after this are not included in the release packaging.
#
# Include dependencies which are only needed for development here.

# Used to test invoking rules with targets in an external repo.
local_repository(
    name = "mappings_test_external_repo",
    path = "tests/mappings/external_repo",
)

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

http_archive(
    name = "platforms",
    sha256 = "218efe8ee736d26a3572663b374a253c012b716d8af0c07e842e82f238a0a7ee",
    urls = [
        "https://github.com/bazelbuild/platforms/releases/download/0.0.10/platforms-0.0.10.tar.gz",
    ],
)

# Find rpmbuild if it exists.
load("@rules_pkg//toolchains/rpm:rpmbuild_configure.bzl", "find_system_rpmbuild")

find_system_rpmbuild(
    name = "rules_pkg_rpmbuild",
    verbose = False,
)

# Needed for making our release notes
load("@rules_pkg//toolchains/git:git_configure.bzl", "experimental_find_system_git")

experimental_find_system_git(
    name = "rules_pkg_git",
    verbose = False,
)

http_archive(
    name = "bazel_stardoc",
    sha256 = "0e1ed4a98f26e718776bd64d053d02bb34d98572ccd03d6ba355112a1205706b",
    urls = [
        "https://mirror.bazel.build/github.com/bazelbuild/stardoc/releases/download/0.7.2/stardoc-0.7.2.tar.gz",
        "https://github.com/bazelbuild/stardoc/releases/download/0.7.2/stardoc-0.7.2.tar.gz",
    ],
)

load("@bazel_stardoc//:setup.bzl", "stardoc_repositories")

stardoc_repositories()

http_archive(
    name = "rules_cc",
    sha256 = "abc605dd850f813bb37004b77db20106a19311a96b2da1c92b789da529d28fe1",
    strip_prefix = "rules_cc-0.0.17",
    urls = ["https://github.com/bazelbuild/rules_cc/releases/download/0.0.17/rules_cc-0.0.17.tar.gz"],
)

load("@rules_cc//cc:repositories.bzl", "rules_cc_dependencies")

rules_cc_dependencies()
