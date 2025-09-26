# Copyright 2024 The Bazel Authors. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""This module is used to construct the config settings in the BUILD file in this same package.
"""

load("@bazel_skylib//lib:selects.bzl", "selects")
load("@bazel_skylib//rules:common_settings.bzl", "BuildSettingInfo")
load("//python/private:text_util.bzl", "render")
load(":version.bzl", "version")

_PYTHON_VERSION_FLAG = Label("//python/config_settings:python_version")
_PYTHON_VERSION_MAJOR_MINOR_FLAG = Label("//python/config_settings:python_version_major_minor")

_DEBUG_ENV_MESSAGE_TEMPLATE = """\
The current configuration rules_python config flags is:
    {flags}

If the value is missing, then the default value is being used, see documentation:
{docs_url}/python/config_settings
"""

# Indicates something needs public visibility so that other generated code can
# access it, but it's not intended for general public usage.
_NOT_ACTUALLY_PUBLIC = ["//visibility:public"]

def construct_config_settings(*, name, default_version, versions, minor_mapping, documented_flags):  # buildifier: disable=function-docstring
    """Create a 'python_version' config flag and construct all config settings used in rules_python.

    This mainly includes the targets that are used in the toolchain and pip hub
    repositories that only match on the 'python_version' flag values.

    Args:
        name: {type}`str` A dummy name value that is no-op for now.
        default_version: {type}`str` the default value for the `python_version` flag.
        versions: {type}`list[str]` A list of versions to build constraint settings for.
        minor_mapping: {type}`dict[str, str]` A mapping from `X.Y` to `X.Y.Z` python versions.
        documented_flags: {type}`list[str]` The labels of the documented settings
            that affect build configuration.
    """
    _ = name  # @unused
    _python_version_flag(
        name = _PYTHON_VERSION_FLAG.name,
        build_setting_default = default_version,
        visibility = ["//visibility:public"],
    )

    _python_version_major_minor_flag(
        name = _PYTHON_VERSION_MAJOR_MINOR_FLAG.name,
        build_setting_default = "",
        visibility = ["//visibility:public"],
    )

    native.config_setting(
        name = "is_python_version_unset",
        flag_values = {_PYTHON_VERSION_FLAG: ""},
        visibility = ["//visibility:public"],
    )

    _reverse_minor_mapping = {full: minor for minor, full in minor_mapping.items()}
    for version in versions:
        minor_version = _reverse_minor_mapping.get(version)
        if not minor_version:
            native.config_setting(
                name = "is_python_{}".format(version),
                flag_values = {":python_version": version},
                visibility = ["//visibility:public"],
            )
            continue

        # Also need to match the minor version when using
        name = "is_python_{}".format(version)
        native.config_setting(
            name = "_" + name,
            flag_values = {":python_version": version},
            visibility = ["//visibility:public"],
        )

        # An alias pointing to an underscore-prefixed config_setting_group
        # is used because config_setting_group creates
        # `is_{version}_N` targets, which are easily confused with the
        # `is_{minor}.{micro}` (dot) targets.
        selects.config_setting_group(
            name = "_{}_group".format(name),
            match_any = [
                ":_is_python_{}".format(version),
                ":is_python_{}".format(minor_version),
            ],
            visibility = ["//visibility:private"],
        )
        native.alias(
            name = name,
            actual = "_{}_group".format(name),
            visibility = ["//visibility:public"],
        )

    # This matches the raw flag value, e.g. --//python/config_settings:python_version=3.8
    # It's private because matching the concept of e.g. "3.8" value is done
    # using the `is_python_X.Y` config setting group, which is aware of the
    # minor versions that could match instead.
    for minor in minor_mapping.keys():
        native.config_setting(
            name = "is_python_{}".format(minor),
            flag_values = {_PYTHON_VERSION_MAJOR_MINOR_FLAG: minor},
            visibility = ["//visibility:public"],
        )

    _current_config(
        name = "current_config",
        build_setting_default = "",
        settings = documented_flags + [_PYTHON_VERSION_FLAG.name],
        visibility = ["//visibility:private"],
    )
    native.config_setting(
        name = "is_not_matching_current_config",
        # We use the rule above instead of @platforms//:incompatible so that the
        # printing of the current env always happens when the _current_config rule
        # is executed.
        #
        # NOTE: This should in practise only happen if there is a missing compatible
        # `whl_library` in the hub repo created by `pip.parse`.
        flag_values = {"current_config": "will-never-match"},
        # Only public so that PyPI hub repo can access it
        visibility = _NOT_ACTUALLY_PUBLIC,
    )

    libc = Label("//python/config_settings:py_linux_libc")
    native.config_setting(
        name = "_is_py_linux_libc_glibc",
        flag_values = {libc: "glibc"},
        visibility = _NOT_ACTUALLY_PUBLIC,
    )
    native.config_setting(
        name = "_is_py_linux_libc_musl",
        flag_values = {libc: "musl"},
        visibility = _NOT_ACTUALLY_PUBLIC,
    )
    freethreaded = Label("//python/config_settings:py_freethreaded")
    native.config_setting(
        name = "_is_py_freethreaded_yes",
        flag_values = {freethreaded: "yes"},
        visibility = _NOT_ACTUALLY_PUBLIC,
    )
    native.config_setting(
        name = "_is_py_freethreaded_no",
        flag_values = {freethreaded: "no"},
        visibility = _NOT_ACTUALLY_PUBLIC,
    )

def _python_version_flag_impl(ctx):
    value = ctx.build_setting_value
    return [
        # BuildSettingInfo is the original provider returned, so continue to
        # return it for compatibility
        BuildSettingInfo(value = value),
        # FeatureFlagInfo is returned so that config_setting respects the value
        # as returned by this rule instead of as originally seen on the command
        # line.
        # It is also for Google compatibility, which expects the FeatureFlagInfo
        # provider.
        config_common.FeatureFlagInfo(value = value),
    ]

_python_version_flag = rule(
    implementation = _python_version_flag_impl,
    build_setting = config.string(flag = True),
    attrs = {},
)

def _python_version_major_minor_flag_impl(ctx):
    input = _flag_value(ctx.attr._python_version_flag)
    if input:
        ver = version.parse(input)
        value = "{}.{}".format(ver.release[0], ver.release[1])
    else:
        value = ""

    return [config_common.FeatureFlagInfo(value = value)]

_python_version_major_minor_flag = rule(
    implementation = _python_version_major_minor_flag_impl,
    build_setting = config.string(flag = False),
    attrs = {
        "_python_version_flag": attr.label(
            default = _PYTHON_VERSION_FLAG,
        ),
    },
)

def _flag_value(s):
    if config_common.FeatureFlagInfo in s:
        return s[config_common.FeatureFlagInfo].value
    else:
        return s[BuildSettingInfo].value

def _print_current_config_impl(ctx):
    flags = "\n".join([
        "{}: \"{}\"".format(k, v)
        for k, v in sorted({
            str(setting.label): _flag_value(setting)
            for setting in ctx.attr.settings
        }.items())
    ])

    msg = ctx.attr._template.format(
        docs_url = "https://rules-python.readthedocs.io/en/latest/api/rules_python",
        flags = render.indent(flags).lstrip(),
    )
    if ctx.build_setting_value and ctx.build_setting_value != "fail":
        fail("Only 'fail' and empty build setting values are allowed for {}".format(
            str(ctx.label),
        ))
    elif ctx.build_setting_value:
        fail(msg)
    else:
        print(msg)  # buildifier: disable=print

    return [config_common.FeatureFlagInfo(value = "")]

_current_config = rule(
    implementation = _print_current_config_impl,
    build_setting = config.string(flag = True),
    attrs = {
        "settings": attr.label_list(mandatory = True),
        "_template": attr.string(default = _DEBUG_ENV_MESSAGE_TEMPLATE),
    },
)

def is_python_version_at_least(name, **kwargs):
    flag_name = "_{}_flag".format(name)
    native.config_setting(
        name = name,
        flag_values = {
            flag_name: "yes",
        },
    )
    _python_version_at_least(
        name = flag_name,
        visibility = ["//visibility:private"],
        **kwargs
    )

def _python_version_at_least_impl(ctx):
    flag_value = ctx.attr._major_minor[config_common.FeatureFlagInfo].value

    # CI is, somehow, getting an empty string for the current flag value.
    # How isn't clear.
    if not flag_value:
        return [config_common.FeatureFlagInfo(value = "no")]

    current = tuple([
        int(x)
        for x in flag_value.split(".")
    ])
    at_least = tuple([int(x) for x in ctx.attr.at_least.split(".")])

    value = "yes" if current >= at_least else "no"
    return [config_common.FeatureFlagInfo(value = value)]

_python_version_at_least = rule(
    implementation = _python_version_at_least_impl,
    attrs = {
        "at_least": attr.string(mandatory = True),
        "_major_minor": attr.label(default = _PYTHON_VERSION_MAJOR_MINOR_FLAG),
    },
)
