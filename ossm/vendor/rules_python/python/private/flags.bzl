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
load(":enum.bzl", "FlagEnum", "enum")

# Maps "--myflag" to a tuple of:
#
# - the flag's ctx.fragments native API accessor
# -"native|starlark": which definition to use if the flag is available both
#      from ctx.fragments and Starlark
#
# Builds that set --incompatible_remove_ctx_py_fragment or
# --incompatible_remove_ctx_bazel_py_fragment disable ctx.fragments. These
# builds assume flags are solely defined in Starlark.
#
# The "native|starlark" override is only for devs who are testing flag
# Starlarkification. If ctx.fragments.[py|bazel_py] is available and
# a flag is set to "starlark", we exclusively read its starlark version.
#
# See https://github.com/bazel-contrib/rules_python/issues/3252.
_POSSIBLY_NATIVE_FLAGS = {
    "build_python_zip": (lambda ctx: ctx.fragments.py.build_python_zip, "native"),
    "default_to_explicit_init_py": (lambda ctx: ctx.fragments.py.default_to_explicit_init_py, "native"),
    "python_import_all_repositories": (lambda ctx: ctx.fragments.bazel_py.python_import_all_repositories, "native"),
    "python_path": (lambda ctx: ctx.fragments.bazel_py.python_path, "native"),
}

def read_possibly_native_flag(ctx, flag_name):
    """
    Canonical API for reading a Python build flag.

    Flags might be defined in Starlark or native-Bazel. This function reasd flags
    from tbe correct source based on supporting Bazel version and --incompatible*
    flags that disable native references.

    Args:
        ctx: Rule's configuration context.
        flag_name: Name of the flag to read, without preceding "--".

    Returns:
        The flag's value.
    """

    # Bazel 9.0+ can disable these fragments with --incompatible_remove_ctx_py_fragment and
    # --incompatible_remove_ctx_bazel_py_fragment. Disabling them means bazel expects
    # Python to read Starlark flags.
    use_native_def = hasattr(ctx.fragments, "py") and hasattr(ctx.fragments, "bazel_py")

    # Developer override to force the Starlark definition for testing.
    if _POSSIBLY_NATIVE_FLAGS[flag_name][1] == "starlark":
        use_native_def = False
    if use_native_def:
        return _POSSIBLY_NATIVE_FLAGS[flag_name][0](ctx)
    else:
        # Starlark definition of "--foo" is assumed to be a label dependency named "_foo".
        return getattr(ctx.attr, "_" + flag_name + "_flag")[BuildSettingInfo].value

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

def _string_flag_impl(ctx):
    if ctx.attr.override:
        value = ctx.attr.override
    else:
        value = ctx.build_setting_value

    if value not in ctx.attr.values:
        fail((
            "Invalid value for {name}: got {value}, must " +
            "be one of {allowed}"
        ).format(
            name = ctx.label,
            value = value,
            allowed = ctx.attr.values,
        ))

    return [
        BuildSettingInfo(value = value),
        config_common.FeatureFlagInfo(value = value),
    ]

string_flag = rule(
    implementation = _string_flag_impl,
    build_setting = config.string(flag = True),
    attrs = {
        "override": attr.string(),
        "values": attr.string_list(),
    },
)

def _bootstrap_impl_flag_get_value(ctx):
    return ctx.attr._bootstrap_impl_flag[config_common.FeatureFlagInfo].value

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

def _venvs_site_packages_is_enabled(ctx):
    if not ctx.attr.experimental_venvs_site_packages:
        return False
    flag_value = ctx.attr.experimental_venvs_site_packages[BuildSettingInfo].value
    return flag_value == VenvsSitePackages.YES

# Decides if libraries try to use a site-packages layout using venv_symlinks
# buildifier: disable=name-conventions
VenvsSitePackages = FlagEnum(
    # Use venv_symlinks
    YES = "yes",
    # Don't use venv_symlinks
    NO = "no",
    is_enabled = _venvs_site_packages_is_enabled,
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
