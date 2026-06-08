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

def py_repositories(transition_settings = []):
    """Runtime dependencies that users must install.

    This function should be loaded and called in the user's `WORKSPACE`.
    With `bzlmod` enabled, this function is not needed since `MODULE.bazel` handles transitive deps.

    Args:
        transition_settings: A list of labels that terminal rules transition on
            by default.
    """

    # NOTE: The @rules_python_internal repo is special cased by Bazel: it
    # has autoloading disabled. This allows the rules to load from it
    # without triggering recursion.
    maybe(
        internal_config_repo,
        name = "rules_python_internal",
        transition_settings = transition_settings,
    )
    maybe(
        hub_repo,
        name = "pythons_hub",
        minor_mapping = MINOR_MAPPING,
        default_python_version = "",
        python_versions = sorted(TOOL_VERSIONS.keys()),
        toolchain_names = [],
        toolchain_repo_names = {},
        toolchain_target_compatible_with_map = {},
        toolchain_target_settings_map = {},
        toolchain_platform_keys = {},
        toolchain_python_versions = {},
        toolchain_set_python_version_constraints = {},
        host_compatible_repo_names = [],
    )
    http_archive(
        name = "bazel_skylib",
        sha256 = "6e78f0e57de26801f6f564fa7c4a48dc8b36873e416257a92bbb0937eeac8446",
        urls = [
            "https://mirror.bazel.build/github.com/bazelbuild/bazel-skylib/releases/download/1.8.2/bazel-skylib-1.8.2.tar.gz",
            "https://github.com/bazelbuild/bazel-skylib/releases/download/1.8.2/bazel-skylib-1.8.2.tar.gz",
        ],
    )
    http_archive(
        name = "rules_cc",
        sha256 = "b8b918a85f9144c01f6cfe0f45e4f2838c7413961a8ff23bc0c6cdf8bb07a3b6",
        strip_prefix = "rules_cc-0.1.5",
        urls = ["https://github.com/bazelbuild/rules_cc/releases/download/0.1.5/rules_cc-0.1.5.tar.gz"],
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
