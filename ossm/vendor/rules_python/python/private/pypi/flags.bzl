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
load("//python/private:enum.bzl", "enum")

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

def _allow_wheels_flag_impl(ctx):
    input = ctx.attr._setting[BuildSettingInfo].value
    value = "yes" if input in ["auto", "only"] else "no"
    return [config_common.FeatureFlagInfo(value = value)]

_allow_wheels_flag = rule(
    implementation = _allow_wheels_flag_impl,
    attrs = {
        "_setting": attr.label(default = "//python/config_settings:pip_whl"),
    },
    doc = """\
This rule allows us to greatly reduce the number of config setting targets at no cost even
if we are duplicating some of the functionality of the `native.config_setting`.
""",
)
