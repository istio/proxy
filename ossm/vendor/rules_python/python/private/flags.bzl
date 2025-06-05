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

"""Values and helpers for flags.

NOTE: The transitive loads of this should be kept minimal. This avoids loading
unnecessary files when all that are needed are flag definitions.
"""

load("@bazel_skylib//rules:common_settings.bzl", "BuildSettingInfo")
load(":enum.bzl", "enum")

def _FlagEnum_flag_values(self):
    return sorted(self.__members__.values())

def FlagEnum(**kwargs):
    """Define an enum specialized for flags.

    Args:
        **kwargs: members of the enum.

    Returns:
        {type}`FlagEnum` struct. This is an enum with the following extras:
        * `flag_values`: A function that returns a sorted list of the
          flag values (enum `__members__`). Useful for passing to the
          `values` attribute for string flags.
    """
    return enum(
        methods = dict(flag_values = _FlagEnum_flag_values),
        **kwargs
    )

def _AddSrcsToRunfilesFlag_is_enabled(ctx):
    value = ctx.attr._add_srcs_to_runfiles_flag[BuildSettingInfo].value
    if value == AddSrcsToRunfilesFlag.AUTO:
        value = AddSrcsToRunfilesFlag.ENABLED
    return value == AddSrcsToRunfilesFlag.ENABLED

# buildifier: disable=name-conventions
AddSrcsToRunfilesFlag = FlagEnum(
    AUTO = "auto",
    ENABLED = "enabled",
    DISABLED = "disabled",
    is_enabled = _AddSrcsToRunfilesFlag_is_enabled,
)

def _bootstrap_impl_flag_get_value(ctx):
    return ctx.attr._bootstrap_impl_flag[BuildSettingInfo].value

# buildifier: disable=name-conventions
BootstrapImplFlag = enum(
    SYSTEM_PYTHON = "system_python",
    SCRIPT = "script",
    get_value = _bootstrap_impl_flag_get_value,
)

def _precompile_flag_get_effective_value(ctx):
    value = ctx.attr._precompile_flag[BuildSettingInfo].value
    if value == PrecompileFlag.AUTO:
        value = PrecompileFlag.DISABLED
    return value

# Determines if the Python exec tools toolchain should be registered.
# buildifier: disable=name-conventions
ExecToolsToolchainFlag = enum(
    # Enable registering the exec tools toolchain using the hermetic toolchain.
    ENABLED = "enabled",
    # Disable registering the exec tools toolchain using the hermetic toolchain.
    DISABLED = "disabled",
)

# Determines if Python source files should be compiled at build time.
#
# NOTE: The flag value is overridden by the target-level attribute, except
# for the case of `force_enabled` and `forced_disabled`.
# buildifier: disable=name-conventions
PrecompileFlag = enum(
    # Automatically decide the effective value based on environment,
    # target platform, etc.
    AUTO = "auto",
    # Compile Python source files at build time.
    ENABLED = "enabled",
    # Don't compile Python source files at build time.
    DISABLED = "disabled",
    # Like `enabled`, except overrides target-level setting. This is mostly
    # useful for development, testing enabling precompilation more broadly, or
    # as an escape hatch to force all transitive deps to precompile.
    FORCE_ENABLED = "force_enabled",
    # Like `disabled`, except overrides target-level setting. This is useful
    # useful for development, testing enabling precompilation more broadly, or
    # as an escape hatch if build-time compiling is not available.
    FORCE_DISABLED = "force_disabled",
    get_effective_value = _precompile_flag_get_effective_value,
)

def _precompile_source_retention_flag_get_effective_value(ctx):
    value = ctx.attr._precompile_source_retention_flag[BuildSettingInfo].value
    if value == PrecompileSourceRetentionFlag.AUTO:
        value = PrecompileSourceRetentionFlag.KEEP_SOURCE
    return value

# Determines if, when a source file is compiled, if the source file is kept
# in the resulting output or not.
# buildifier: disable=name-conventions
PrecompileSourceRetentionFlag = enum(
    # Automatically decide the effective value based on environment, etc.
    AUTO = "auto",
    # Include the original py source in the output.
    KEEP_SOURCE = "keep_source",
    # Don't include the original py source.
    OMIT_SOURCE = "omit_source",
    get_effective_value = _precompile_source_retention_flag_get_effective_value,
)

def _venvs_use_declare_symlink_flag_get_value(ctx):
    return ctx.attr._venvs_use_declare_symlink_flag[BuildSettingInfo].value

# Decides if the venv created by bootstrap=script uses declare_file() to
# create relative symlinks. Workaround for #2489 (packaging rules not supporting
# declare_link() files).
# buildifier: disable=name-conventions
VenvsUseDeclareSymlinkFlag = FlagEnum(
    # Use declare_file() and relative symlinks in the venv
    YES = "yes",
    # Do not use declare_file() and relative symlinks in the venv
    NO = "no",
    get_value = _venvs_use_declare_symlink_flag_get_value,
)

# Used for matching freethreaded toolchains and would have to be used in wheels
# as well.
# buildifier: disable=name-conventions
FreeThreadedFlag = enum(
    # Use freethreaded python toolchain and wheels.
    YES = "yes",
    # Do not use freethreaded python toolchain and wheels.
    NO = "no",
)

# Determines which libc flavor is preferred when selecting the toolchain and
# linux whl distributions.
#
# buildifier: disable=name-conventions
LibcFlag = FlagEnum(
    # Prefer glibc wheels (e.g. manylinux_2_17_x86_64 or linux_x86_64)
    GLIBC = "glibc",
    # Prefer musl wheels (e.g. musllinux_2_17_x86_64)
    MUSL = "musl",
)
