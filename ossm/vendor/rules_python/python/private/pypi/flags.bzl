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

"""Values and helpers for pip_repository related flags.

NOTE: The transitive loads of this should be kept minimal. This avoids loading
unnecessary files when all that are needed are flag definitions.
"""

load("@bazel_skylib//rules:common_settings.bzl", "BuildSettingInfo", "string_flag")
load("//python/private:common_labels.bzl", "labels")
load("//python/private:enum.bzl", "enum")
load(":env_marker_info.bzl", "EnvMarkerInfo")
load(
    ":pep508_env.bzl",
    "create_env",
    "os_name_select_map",
    "platform_machine_select_map",
    "platform_system_select_map",
    "sys_platform_select_map",
)

# Determines if we should use whls for third party
#
# buildifier: disable=name-conventions
UseWhlFlag = enum(
    # Automatically decide the effective value based on environment, target
    # platform and the presence of distributions for a particular package.
    AUTO = "auto",
    # Do not use `sdist` and fail if there are no available whls suitable for the target platform.
    ONLY = "only",
    # Do not use whl distributions and instead build the whls from `sdist`.
    NO = "no",
)

# Determines whether universal wheels should be preferred over arch platform specific ones.
#
# buildifier: disable=name-conventions
UniversalWhlFlag = enum(
    # Prefer platform-specific wheels over universal wheels.
    ARCH = "arch",
    # Prefer universal wheels over platform-specific wheels.
    UNIVERSAL = "universal",
)

_STRING_FLAGS = [
    "dist",
    "whl_plat",
    "whl_plat_py3",
    "whl_plat_py3_abi3",
    "whl_plat_pycp3x",
    "whl_plat_pycp3x_abi3",
    "whl_plat_pycp3x_abicp",
    "whl_py3",
    "whl_py3_abi3",
    "whl_pycp3x",
    "whl_pycp3x_abi3",
    "whl_pycp3x_abicp",
]

INTERNAL_FLAGS = [
    "whl",
] + _STRING_FLAGS

def define_pypi_internal_flags(name):
    """define internal PyPI flags used in PyPI hub repository by pkg_aliases.

    Args:
        name: not used
    """
    for flag in _STRING_FLAGS:
        string_flag(
            name = "_internal_pip_" + flag,
            build_setting_default = "",
            values = [""],
            visibility = ["//visibility:public"],
        )

    _allow_wheels_flag(
        name = "_internal_pip_whl",
        visibility = ["//visibility:public"],
    )

    _default_env_marker_config(
        name = "_pip_env_marker_default_config",
    )

def _allow_wheels_flag_impl(ctx):
    input = ctx.attr._setting[BuildSettingInfo].value
    value = "yes" if input in ["auto", "only"] else "no"
    return [config_common.FeatureFlagInfo(value = value)]

_allow_wheels_flag = rule(
    implementation = _allow_wheels_flag_impl,
    attrs = {
        "_setting": attr.label(default = labels.PIP_WHL),
    },
    doc = """
This rule allows us to greatly reduce the number of config setting targets at no cost even
if we are duplicating some of the functionality of the `native.config_setting`.
""",
)

def _default_env_marker_config(**kwargs):
    _env_marker_config(
        os_name = select(os_name_select_map),
        sys_platform = select(sys_platform_select_map),
        platform_machine = select(platform_machine_select_map),
        platform_system = select(platform_system_select_map),
        platform_release = select({
            "@platforms//os:osx": "USE_OSX_VERSION_FLAG",
            "//conditions:default": "",
        }),
        **kwargs
    )

def _env_marker_config_impl(ctx):
    env = create_env()
    env["os_name"] = ctx.attr.os_name
    env["sys_platform"] = ctx.attr.sys_platform
    env["platform_machine"] = ctx.attr.platform_machine

    # NOTE: Platform release for Android will be Android version:
    # https://peps.python.org/pep-0738/#platform
    # Similar for iOS:
    # https://peps.python.org/pep-0730/#platform
    platform_release = ctx.attr.platform_release
    if platform_release == "USE_OSX_VERSION_FLAG":
        platform_release = _get_flag(ctx.attr._pip_whl_osx_version_flag)
    env["platform_release"] = platform_release
    env["platform_system"] = ctx.attr.platform_system

    # NOTE: We intentionally do not call set_missing_env_defaults() here because
    # `env_marker_setting()` computes missing values using the toolchain.
    return [EnvMarkerInfo(env = env)]

_env_marker_config = rule(
    implementation = _env_marker_config_impl,
    attrs = {
        "os_name": attr.string(),
        "platform_machine": attr.string(),
        "platform_release": attr.string(),
        "platform_system": attr.string(),
        "sys_platform": attr.string(),
        "_pip_whl_osx_version_flag": attr.label(
            default = labels.PIP_WHL_OSX_VERSION,
            providers = [[BuildSettingInfo], [config_common.FeatureFlagInfo]],
        ),
    },
)

def _get_flag(t):
    if config_common.FeatureFlagInfo in t:
        return t[config_common.FeatureFlagInfo].value
    if BuildSettingInfo in t:
        return t[BuildSettingInfo].value
    fail("Should not occur: {} does not have necessary providers")
