# Copyright 2022 The Bazel Authors. All rights reserved.
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

"""Create the toolchain defs in a BUILD.bazel file."""

load("@bazel_skylib//lib:selects.bzl", "selects")
load("@platforms//host:constraints.bzl", "HOST_CONSTRAINTS")
load(":text_util.bzl", "render")
load(
    ":toolchain_types.bzl",
    "EXEC_TOOLS_TOOLCHAIN_TYPE",
    "PY_CC_TOOLCHAIN_TYPE",
    "TARGET_TOOLCHAIN_TYPE",
)

_IS_EXEC_TOOLCHAIN_ENABLED = Label("//python/config_settings:is_exec_tools_toolchain_enabled")

# buildifier: disable=unnamed-macro
def py_toolchain_suite(
        *,
        prefix,
        user_repository_name,
        python_version,
        set_python_version_constraint,
        flag_values,
        target_settings = [],
        target_compatible_with = []):
    """For internal use only.

    Args:
        prefix: Prefix for toolchain target names.
        user_repository_name: The name of the repository with the toolchain
            implementation (it's assumed to have particular target names within
            it). Does not include the leading "@".
        python_version: The full (X.Y.Z) version of the interpreter.
        set_python_version_constraint: True or False as a string.
        flag_values: Extra flag values to match for this toolchain. These
            are prepended to target_settings.
        target_settings: Extra target_settings to match for this toolchain.
        target_compatible_with: list constraints the toolchains are compatible with.
    """

    # We have to use a String value here because bzlmod is passing in a
    # string as we cannot have list of bools in build rule attributes.
    # This if statement does not appear to work unless it is in the
    # toolchain file.
    if set_python_version_constraint in ["True", "False"]:
        major_minor, _, _ = python_version.rpartition(".")
        python_versions = [major_minor, python_version]
        if set_python_version_constraint == "False":
            python_versions.append("")

        match_any = []
        for i, v in enumerate(python_versions):
            name = "{prefix}_{python_version}_{i}".format(
                prefix = prefix,
                python_version = python_version,
                i = i,
            )
            match_any.append(name)
            native.config_setting(
                name = name,
                flag_values = flag_values | {
                    Label("@rules_python//python/config_settings:python_version"): v,
                },
                visibility = ["//visibility:private"],
            )

        name = "{prefix}_version_setting_{python_version}".format(
            prefix = prefix,
            python_version = python_version,
            visibility = ["//visibility:private"],
        )
        selects.config_setting_group(
            name = name,
            match_any = match_any,
            visibility = ["//visibility:private"],
        )
        target_settings = [name] + target_settings
    else:
        fail(("Invalid set_python_version_constraint value: got {} {}, wanted " +
              "either the string 'True' or the string 'False'; " +
              "(did you convert bool to string?)").format(
            type(set_python_version_constraint),
            repr(set_python_version_constraint),
        ))

    _internal_toolchain_suite(
        prefix = prefix,
        runtime_repo_name = user_repository_name,
        target_settings = target_settings,
        target_compatible_with = target_compatible_with,
        exec_compatible_with = [],
    )

def _internal_toolchain_suite(
        prefix,
        runtime_repo_name,
        target_compatible_with,
        target_settings,
        exec_compatible_with):
    native.toolchain(
        name = "{prefix}_toolchain".format(prefix = prefix),
        toolchain = "@{runtime_repo_name}//:python_runtimes".format(
            runtime_repo_name = runtime_repo_name,
        ),
        toolchain_type = TARGET_TOOLCHAIN_TYPE,
        target_settings = target_settings,
        target_compatible_with = target_compatible_with,
        exec_compatible_with = exec_compatible_with,
    )

    native.toolchain(
        name = "{prefix}_py_cc_toolchain".format(prefix = prefix),
        toolchain = "@{runtime_repo_name}//:py_cc_toolchain".format(
            runtime_repo_name = runtime_repo_name,
        ),
        toolchain_type = PY_CC_TOOLCHAIN_TYPE,
        target_settings = target_settings,
        target_compatible_with = target_compatible_with,
        exec_compatible_with = exec_compatible_with,
    )

    native.toolchain(
        name = "{prefix}_py_exec_tools_toolchain".format(prefix = prefix),
        toolchain = "@{runtime_repo_name}//:py_exec_tools_toolchain".format(
            runtime_repo_name = runtime_repo_name,
        ),
        toolchain_type = EXEC_TOOLS_TOOLCHAIN_TYPE,
        target_settings = select({
            _IS_EXEC_TOOLCHAIN_ENABLED: target_settings,
            # Whatever the default is, it has to map to a `config_setting`
            # that will never match. Since the default branch is only taken if
            # _IS_EXEC_TOOLCHAIN_ENABLED is false, then it will never match
            # when later evaluated during toolchain resolution.
            # Note that @platforms//:incompatible can't be used here because
            # the RHS must be a `config_setting`.
            "//conditions:default": [_IS_EXEC_TOOLCHAIN_ENABLED],
        }),
        exec_compatible_with = target_compatible_with,
    )

    # NOTE: When adding a new toolchain, for WORKSPACE builds to see the
    # toolchain, the name must be added to the native.register_toolchains()
    # call in python/repositories.bzl. Bzlmod doesn't need anything; it will
    # register `:all`.

def define_local_toolchain_suites(
        name,
        version_aware_repo_names,
        version_unaware_repo_names,
        repo_exec_compatible_with,
        repo_target_compatible_with,
        repo_target_settings):
    """Define toolchains for `local_runtime_repo` backed toolchains.

    This generates `toolchain` targets that can be registered using `:all`. The
    specific names of the toolchain targets are not defined. The priority order
    of the toolchains is the order that is passed in, with version-aware having
    higher priority than version-unaware.

    Args:
        name: `str` Unused; only present to satisfy tooling.
        version_aware_repo_names: `list[str]` of the repo names that will have
            version-aware toolchains defined.
        version_unaware_repo_names: `list[str]` of the repo names that will have
            version-unaware toolchains defined.
        repo_target_settings: {type}`dict[str, list[str]]` mapping of repo names
            to string labels that are added to the `target_settings` for the
            respective repo's toolchain.
        repo_target_compatible_with: {type}`dict[str, list[str]]` mapping of repo names
            to string labels that are added to the `target_compatible_with` for
            the respective repo's toolchain.
        repo_exec_compatible_with: {type}`dict[str, list[str]]` mapping of repo names
            to string labels that are added to the `exec_compatible_with` for
            the respective repo's toolchain.
    """

    i = 0
    for i, repo in enumerate(version_aware_repo_names, start = i):
        target_settings = ["@{}//:is_matching_python_version".format(repo)]

        if repo_target_settings.get(repo):
            selects.config_setting_group(
                name = "_{}_user_guard".format(repo),
                match_all = repo_target_settings.get(repo, []) + target_settings,
            )
            target_settings = ["_{}_user_guard".format(repo)]
        _internal_toolchain_suite(
            prefix = render.left_pad_zero(i, 4),
            runtime_repo_name = repo,
            target_compatible_with = _get_local_toolchain_target_compatible_with(
                repo,
                repo_target_compatible_with,
            ),
            target_settings = target_settings,
            exec_compatible_with = repo_exec_compatible_with.get(repo, []),
        )

    # The version unaware entries must go last because they will match any Python
    # version.
    for i, repo in enumerate(version_unaware_repo_names, start = i + 1):
        _internal_toolchain_suite(
            prefix = render.left_pad_zero(i, 4) + "_default",
            runtime_repo_name = repo,
            target_compatible_with = _get_local_toolchain_target_compatible_with(
                repo,
                repo_target_compatible_with,
            ),
            # We don't call _get_local_toolchain_target_settings because that
            # will add the version matching condition by default.
            target_settings = repo_target_settings.get(repo, []),
            exec_compatible_with = repo_exec_compatible_with.get(repo, []),
        )

def _get_local_toolchain_target_compatible_with(repo, repo_target_compatible_with):
    if repo in repo_target_compatible_with:
        target_compatible_with = repo_target_compatible_with[repo]
        if "HOST_CONSTRAINTS" in target_compatible_with:
            target_compatible_with.remove("HOST_CONSTRAINTS")
            target_compatible_with.extend(HOST_CONSTRAINTS)
    else:
        target_compatible_with = ["@{}//:os".format(repo)]
    return target_compatible_with
