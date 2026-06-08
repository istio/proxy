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

    whl_from_dir_repo(
        name = "pkgutil_nspkg1_whl",
        root = "//tests/repos/pkgutil_nspkg1:BUILD.bazel",
        output = "pkgutil_nspkg1-1.0-any-none-any.whl",
    )
    whl_library(
        name = "pkgutil_nspkg1",
        whl_file = "@pkgutil_nspkg1_whl//:pkgutil_nspkg1-1.0-any-none-any.whl",
        requirement = "pkgutil_nspkg1",
        enable_implicit_namespace_pkgs = False,
    )

    whl_from_dir_repo(
        name = "pkgutil_nspkg2_whl",
        root = "//tests/repos/pkgutil_nspkg2:BUILD.bazel",
        output = "pkgutil_nspkg2-1.0-any-none-any.whl",
    )
    whl_library(
        name = "pkgutil_nspkg2",
        whl_file = "@pkgutil_nspkg2_whl//:pkgutil_nspkg2-1.0-any-none-any.whl",
        requirement = "pkgutil_nspkg2",
        enable_implicit_namespace_pkgs = False,
    )

    _whl_library_from_dir(
        name = "whl_library_extras_direct_dep",
        root = "//tests/pypi/whl_library/testdata/pkg:BUILD.bazel",
        output = "pkg-1.0-any-none-any.whl",
        requirement = "pkg[optional]",
        # The following is necessary to enable pipstar and make tests faster
        config_load = "@rules_python//tests/pypi/whl_library/testdata:packages.bzl",
        dep_template = "@whl_library_extras_{name}//:{target}",
    )
    _whl_library_from_dir(
        name = "whl_library_extras_optional_dep",
        root = "//tests/pypi/whl_library/testdata/optional_dep:BUILD.bazel",
        output = "optional_dep-1.0-any-none-any.whl",
        requirement = "optional_dep",
        # The following is necessary to enable pipstar and make tests faster
        config_load = "@rules_python//tests/pypi/whl_library/testdata:packages.bzl",
    )

def _whl_library_from_dir(*, name, output, root, **kwargs):
    whl_from_dir_repo(
        name = "{}_whl".format(name),
        root = root,
        output = output,
    )
    whl_library(
        name = name,
        whl_file = "@{}_whl//:{}".format(name, output),
        **kwargs
    )

internal_dev_deps = module_extension(
    implementation = _internal_dev_deps_impl,
    doc = "This extension creates internal rules_python dev dependencies.",
)
