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
"""Module extension for internal dev_dependency=True setup."""

load("@bazel_ci_rules//:rbe_repo.bzl", "rbe_preconfig")
load("//python/private/pypi:whl_library.bzl", "whl_library")
load("//tests/support/whl_from_dir:whl_from_dir_repo.bzl", "whl_from_dir_repo")
load(":runtime_env_repo.bzl", "runtime_env_repo")

def _internal_dev_deps_impl(mctx):
    _ = mctx  # @unused

    # Creates a default toolchain config for RBE.
    # Use this as is if you are using the rbe_ubuntu16_04 container,
    # otherwise refer to RBE docs.
    rbe_preconfig(
        name = "buildkite_config",
        toolchain = "ubuntu2204",
    )
    runtime_env_repo(name = "rules_python_runtime_env_tc_info")

    # Setup for //tests/whl_with_build_files
    whl_from_dir_repo(
        name = "whl_with_build_files",
        root = "//tests/whl_with_build_files:testdata/BUILD.bazel",
        output = "somepkg-1.0-any-none-any.whl",
    )
    whl_library(
        name = "somepkg_with_build_files",
        whl_file = "@whl_with_build_files//:somepkg-1.0-any-none-any.whl",
        requirement = "somepkg",
    )

    # Setup for //tests/implicit_namespace_packages
    whl_from_dir_repo(
        name = "implicit_namespace_ns_sub1_whl",
        root = "//tests/implicit_namespace_packages:testdata/ns-sub1/BUILD.bazel",
        output = "ns_sub1-1.0-any-none-any.whl",
    )
    whl_library(
        name = "implicit_namespace_ns_sub1",
        whl_file = "@implicit_namespace_ns_sub1_whl//:ns_sub1-1.0-any-none-any.whl",
        requirement = "ns-sub1",
        enable_implicit_namespace_pkgs = False,
    )

    whl_from_dir_repo(
        name = "implicit_namespace_ns_sub2_whl",
        root = "//tests/implicit_namespace_packages:testdata/ns-sub2/BUILD.bazel",
        output = "ns_sub2-1.0-any-none-any.whl",
    )
    whl_library(
        name = "implicit_namespace_ns_sub2",
        whl_file = "@implicit_namespace_ns_sub2_whl//:ns_sub2-1.0-any-none-any.whl",
        requirement = "ns-sub2",
        enable_implicit_namespace_pkgs = False,
    )

internal_dev_deps = module_extension(
    implementation = _internal_dev_deps_impl,
    doc = "This extension creates internal rules_python dev dependencies.",
)
