# Copyright 2024 The Bazel Authors. All rights reserved.
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

"""This file contains macros to be called during WORKSPACE evaluation."""

load("@bazel_tools//tools/build_defs/repo:http.bzl", _http_archive = "http_archive")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")
load("//python:versions.bzl", "MINOR_MAPPING", "TOOL_VERSIONS")
load("//python/private/pypi:deps.bzl", "pypi_deps")
load(":internal_config_repo.bzl", "internal_config_repo")
load(":pythons_hub.bzl", "hub_repo")

def http_archive(**kwargs):
    maybe(_http_archive, **kwargs)

def py_repositories():
    """Runtime dependencies that users must install.

    This function should be loaded and called in the user's `WORKSPACE`.
    With `bzlmod` enabled, this function is not needed since `MODULE.bazel` handles transitive deps.
    """
    maybe(
        internal_config_repo,
        name = "rules_python_internal",
    )
    maybe(
        hub_repo,
        name = "pythons_hub",
        minor_mapping = MINOR_MAPPING,
        default_python_version = "",
        toolchain_prefixes = [],
        toolchain_python_versions = [],
        toolchain_set_python_version_constraints = [],
        toolchain_user_repository_names = [],
        python_versions = sorted(TOOL_VERSIONS.keys()),
    )
    http_archive(
        name = "bazel_skylib",
        sha256 = "d00f1389ee20b60018e92644e0948e16e350a7707219e7a390fb0a99b6ec9262",
        urls = [
            "https://mirror.bazel.build/github.com/bazelbuild/bazel-skylib/releases/download/1.7.0/bazel-skylib-1.7.0.tar.gz",
            "https://github.com/bazelbuild/bazel-skylib/releases/download/1.7.0/bazel-skylib-1.7.0.tar.gz",
        ],
    )
    http_archive(
        name = "rules_cc",
        sha256 = "4b12149a041ddfb8306a8fd0e904e39d673552ce82e4296e96fac9cbf0780e59",
        strip_prefix = "rules_cc-0.1.0",
        urls = ["https://github.com/bazelbuild/rules_cc/releases/download/0.1.0/rules_cc-0.1.0.tar.gz"],
    )

    # Needed by rules_cc, triggered by @rules_java_prebuilt in Bazel by using @rules_cc//cc:defs.bzl
    # NOTE: This name must be com_google_protobuf until Bazel drops WORKSPACE
    # support; Bazel itself has references to com_google_protobuf.
    http_archive(
        name = "com_google_protobuf",
        sha256 = "23082dca1ca73a1e9c6cbe40097b41e81f71f3b4d6201e36c134acc30a1b3660",
        url = "https://github.com/protocolbuffers/protobuf/releases/download/v29.0-rc2/protobuf-29.0-rc2.zip",
        strip_prefix = "protobuf-29.0-rc2",
    )
    pypi_deps()
