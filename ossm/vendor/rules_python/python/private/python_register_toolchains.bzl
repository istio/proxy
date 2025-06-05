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

"""This file contains repository rules and macros to support toolchain registration.
"""

load(
    "//python:versions.bzl",
    "DEFAULT_RELEASE_BASE_URL",
    "MINOR_MAPPING",
    "PLATFORMS",
    "TOOL_VERSIONS",
    "get_release_info",
)
load(":coverage_deps.bzl", "coverage_dep")
load(":full_version.bzl", "full_version")
load(":python_repository.bzl", "python_repository")
load(
    ":toolchains_repo.bzl",
    "host_toolchain",
    "toolchain_aliases",
    "toolchains_repo",
)

# Wrapper macro around everything above, this is the primary API.
def python_register_toolchains(
        name,
        python_version,
        register_toolchains = True,
        register_coverage_tool = False,
        set_python_version_constraint = False,
        tool_versions = None,
        minor_mapping = None,
        **kwargs):
    """Convenience macro for users which does typical setup.

    With `bzlmod` enabled, this function is not needed since `rules_python` is
    handling everything. In order to override the default behaviour from the
    root module one can see the docs for the {rule}`python` extension.

    - Create a repository for each built-in platform like "python_3_8_linux_amd64" -
      this repository is lazily fetched when Python is needed for that platform.
    - Create a repository exposing toolchains for each platform like
      "python_platforms".
    - Register a toolchain pointing at each platform.

    Users can avoid this macro and do these steps themselves, if they want more
    control.

    Args:
        name: {type}`str` base name for all created repos, e.g. "python_3_8".
        python_version: {type}`str` the Python version.
        register_toolchains: {type}`bool` Whether or not to register the downloaded toolchains.
        register_coverage_tool: {type}`bool` Whether or not to register the
            downloaded coverage tool to the toolchains.
        set_python_version_constraint: {type}`bool` When set to `True`,
            `target_compatible_with` for the toolchains will include a version
            constraint.
        tool_versions: {type}`dict` contains a mapping of version with SHASUM
            and platform info. If not supplied, the defaults in
            python/versions.bzl will be used.
        minor_mapping: {type}`dict[str, str]` contains a mapping from `X.Y` to `X.Y.Z`
            version.
        **kwargs: passed to each {obj}`python_repository` call.

    Returns:
        On bzlmod this returns the loaded platform labels. Otherwise None.
    """
    bzlmod_toolchain_call = kwargs.pop("_internal_bzlmod_toolchain_call", False)
    if bzlmod_toolchain_call:
        register_toolchains = False

    base_url = kwargs.pop("base_url", DEFAULT_RELEASE_BASE_URL)
    tool_versions = tool_versions or TOOL_VERSIONS
    minor_mapping = minor_mapping or MINOR_MAPPING

    python_version = full_version(version = python_version, minor_mapping = minor_mapping)

    toolchain_repo_name = "{name}_toolchains".format(name = name)

    # When using unreleased Bazel versions, the version is an empty string
    if native.bazel_version:
        bazel_major = int(native.bazel_version.split(".")[0])
        if bazel_major < 6:
            if register_coverage_tool:
                # buildifier: disable=print
                print((
                    "WARNING: ignoring register_coverage_tool=True when " +
                    "registering @{name}: Bazel 6+ required, got {version}"
                ).format(
                    name = name,
                    version = native.bazel_version,
                ))
            register_coverage_tool = False

    loaded_platforms = []
    for platform in PLATFORMS.keys():
        sha256 = tool_versions[python_version]["sha256"].get(platform, None)
        if not sha256:
            continue

        loaded_platforms.append(platform)
        (release_filename, urls, strip_prefix, patches, patch_strip) = get_release_info(platform, python_version, base_url, tool_versions)

        # allow passing in a tool version
        coverage_tool = None
        coverage_tool = tool_versions[python_version].get("coverage_tool", {}).get(platform, None)
        if register_coverage_tool and coverage_tool == None:
            coverage_tool = coverage_dep(
                name = "{name}_{platform}_coverage".format(
                    name = name,
                    platform = platform,
                ),
                python_version = python_version,
                platform = platform,
                visibility = ["@{name}_{platform}//:__subpackages__".format(
                    name = name,
                    platform = platform,
                )],
            )

        python_repository(
            name = "{name}_{platform}".format(
                name = name,
                platform = platform,
            ),
            sha256 = sha256,
            patches = patches,
            patch_strip = patch_strip,
            platform = platform,
            python_version = python_version,
            release_filename = release_filename,
            urls = urls,
            strip_prefix = strip_prefix,
            coverage_tool = coverage_tool,
            **kwargs
        )
        if register_toolchains:
            native.register_toolchains("@{toolchain_repo_name}//:{platform}_toolchain".format(
                toolchain_repo_name = toolchain_repo_name,
                platform = platform,
            ))
            native.register_toolchains("@{toolchain_repo_name}//:{platform}_py_cc_toolchain".format(
                toolchain_repo_name = toolchain_repo_name,
                platform = platform,
            ))
            native.register_toolchains("@{toolchain_repo_name}//:{platform}_py_exec_tools_toolchain".format(
                toolchain_repo_name = toolchain_repo_name,
                platform = platform,
            ))

    host_toolchain(
        name = name + "_host",
        platforms = loaded_platforms,
        python_version = python_version,
    )

    toolchain_aliases(
        name = name,
        python_version = python_version,
        user_repository_name = name,
        platforms = loaded_platforms,
    )

    # in bzlmod we write out our own toolchain repos
    if bzlmod_toolchain_call:
        return loaded_platforms

    toolchains_repo(
        name = toolchain_repo_name,
        python_version = python_version,
        set_python_version_constraint = set_python_version_constraint,
        user_repository_name = name,
        platforms = loaded_platforms,
    )
    return None
