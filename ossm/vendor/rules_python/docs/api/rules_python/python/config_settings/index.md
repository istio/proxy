:::{default-domain} bzl
:::
:::{bzl:currentfile} //python/config_settings:BUILD.bazel
:::

# //python/config_settings

:::{bzl:flag} add_srcs_to_runfiles
Determines if the `srcs` of targets are added to their runfiles.

More specifically, the sources added to runfiles are the `.py` files in `srcs`.
If precompiling is performed, it is the `.py` files that are kept according
to {obj}`precompile_source_retention`.

Values:
* `auto`: (default) Automatically decide the effective value; the current
  behavior is `disabled`.
* `disabled`: Don't add `srcs` to a target's runfiles.
* `enabled`:  Add `srcs` to a target's runfiles.
::::{versionadded} 0.37.0
::::
::::{deprecated} 0.37.0
This is a transition flag and will be removed in a subsequent release.
::::
:::

:::{bzl:flag} python_version
Determines the default hermetic Python toolchain version. This can be set to
one of the values that `rules_python` maintains.
:::

:::{bzl:target} python_version_major_minor
Parses the value of the `python_version` and transforms it into a `X.Y` value.
:::

:::{bzl:target} is_python_*
config_settings to match Python versions

The name pattern is `is_python_X.Y` (to match major.minor) and `is_python_X.Y.Z`
(to match major.minor.patch).

Note that the set of available targets depends on the configured
`TOOL_VERSIONS`. Versions may not always be available if the root module has
customized them, or as older Python versions are removed from rules_python's set
of builtin, known versions.

If you need to match a version that isn't present, then you have two options:
1. Manually define a `config_setting` and have it match {obj}`--python_version`
   or {obj}`python_version_major_minor`. This works best when you don't control the
   root module, or don't want to rely on the MODULE.bazel configuration. Such
   a config settings would look like:
   ```
   # Match any 3.5 version
   config_setting(
       name = "is_python_3.5",
       flag_values = {
           "@rules_python//python/config_settings:python_version_major_minor": "3.5",
       }
   )
   # Match exactly 3.5.1
   config_setting(
       name = "is_python_3.5.1",
       flag_values = {
           "@rules_python//python/config_settings:python_version": "3.5.1",
       }
   )
   ```

2. Use {obj}`python.single_override` to re-introduce the desired version so
   that the corresponding `//python/config_setting:is_python_XXX` target is
   generated.
:::

::::{bzl:flag} exec_tools_toolchain
Determines if the {obj}`exec_tools_toolchain_type` toolchain is enabled.

:::{note}
* Note that this only affects the rules_python generated toolchains.
:::

Values:

* `enabled`: Allow matching of the registered toolchains at build time.
* `disabled`: Prevent the toolchain from being matched at build time.

:::{versionadded} 0.33.2
:::
::::

::::{bzl:flag} precompile
Determines if Python source files should be compiled at build time.

:::{note}
The flag value is overridden by the target level {attr}`precompile` attribute,
except for the case of `force_enabled` and `forced_disabled`.
:::

Values:

* `auto`: (default) Automatically decide the effective value based on environment,
  target platform, etc.
* `enabled`: Compile Python source files at build time.
* `disabled`: Don't compile Python source files at build time.
* `force_enabled`: Like `enabled`, except overrides target-level setting. This
  is mostly useful for development, testing enabling precompilation more
  broadly, or as an escape hatch if build-time compiling is not available.
* `force_disabled`: Like `disabled`, except overrides target-level setting. This
  is useful useful for development, testing enabling precompilation more
  broadly, or as an escape hatch if build-time compiling is not available.
:::{versionadded} 0.33.0
:::
:::{versionchanged} 0.37.0
The `if_generated_source` value was removed
:::
::::

::::{bzl:flag} precompile_source_retention
Determines, when a source file is compiled, if the source file is kept
in the resulting output or not.

:::{note}
This flag is overridden by the target level `precompile_source_retention`
attribute.
:::

Values:

* `auto`: (default) Automatically decide the effective value based on environment,
  target platform, etc.
* `keep_source`: Include the original Python source.
* `omit_source`: Don't include the orignal py source.

:::{versionadded} 0.33.0
:::
:::{versionadded} 0.36.0
The `auto` value
:::
:::{versionchanged} 0.37.0
The `omit_if_generated_source` value was removed
::::

::::{bzl:flag} py_linux_libc
Set what libc is used for the target platform. This will affect which whl binaries will be pulled and what toolchain will be auto-detected. Currently `rules_python` only supplies toolchains compatible with `glibc`.

Values:
* `glibc`: Use `glibc`, default.
* `muslc`: Use `muslc`.
:::{versionadded} 0.33.0
:::
::::

::::{bzl:flag} py_freethreaded
Set whether to use an interpreter with the experimental freethreaded option set to true.

Values:
* `no`: Use regular Python toolchains, default.
* `yes`: Use the experimental Python toolchain with freethreaded compile option enabled.
:::{versionadded} 0.38.0
:::
::::

::::{bzl:flag} pip_env_marker_config
The target that provides the values for pip env marker evaluation.

Default: `//python/config_settings:_pip_env_marker_default_config`

This flag points to a target providing {obj}`EnvMarkerInfo`, which determines
the values used when environment markers are resolved at build time.

:::{versionadded} 1.5.0
:::
::::

::::{bzl:flag} pip_whl
Set what distributions are used in the `pip` integration.

Values:
* `auto`: Prefer `whl` distributions if they are compatible with a target
  platform, but fallback to `sdist`. This is the default.
* `only`: Only use `whl` distributions and error out if it is not available.
* `no`: Only use `sdist` distributions. The wheels will be built non-hermetically in the `whl_library` repository rule.
:::{versionadded} 0.33.0
:::
::::

::::{bzl:flag} pip_whl_osx_arch
Set what wheel types we should prefer when building on the OSX platform.

Values:
* `arch`: Prefer architecture specific wheels.
* `universal`: Prefer universal wheels that usually are bigger and contain binaries for both, Intel and ARM architectures in the same wheel.
:::{versionadded} 0.33.0
:::
::::

::::{bzl:flag} pip_whl_glibc_version
Set the minimum `glibc` version that the `py_binary` using `whl` distributions from a PyPI index should support.

Values:
* `""`: Select the lowest available version of each wheel giving you the maximum compatibility. This is the default.
* `X.Y`: The string representation of a `glibc` version. The allowed values depend on the `requirements.txt` lock file contents.
:::{versionadded} 0.33.0
:::
::::

::::{bzl:flag} pip_whl_muslc_version
Set the minimum `muslc` version that the `py_binary` using `whl` distributions from a PyPI index should support.

Values:
* `""`: Select the lowest available version of each wheel giving you the maximum compatibility. This is the default.
* `X.Y`: The string representation of a `muslc` version. The allowed values depend on the `requirements.txt` lock file contents.
:::{versionadded} 0.33.0
:::
::::

::::{bzl:flag} pip_whl_osx_version
Set the minimum `osx` version that the `py_binary` using `whl` distributions from a PyPI index should support.

Values:
* `""`: Select the lowest available version of each wheel giving you the maximum compatibility. This is the default.
* `X.Y`: The string representation of the MacOS version. The allowed values depend on the `requirements.txt` lock file contents.

:::{versionadded} 0.33.0
:::
::::


::::

:::{flag} venvs_site_packages

Determines if libraries use a site-packages layout for their files.

Note this flag only affects PyPI dependencies of `--bootstrap_impl=script` binaries

:::{include} /_includes/experimental_api.md
:::


Values:
* `no` (default): Make libraries importable by adding to `sys.path`
* `yes`: Make libraries importable by creating paths in a binary's site-packages directory.
::::

::::{bzl:flag} bootstrap_impl
Determine how programs implement their startup process.

Values:
* `system_python`: (default) Use a bootstrap that requires a system Python available
  in order to start programs. This requires
  {obj}`PyRuntimeInfo.bootstrap_template` to be a Python program.
* `script`: Use a bootstrap that uses an arbitrary executable script (usually a
  shell script) instead of requiring it be a Python program.

:::{note}
The `script` bootstrap requires the toolchain to provide the `PyRuntimeInfo`
provider from `rules_python`. This loosely translates to using Bazel 7+ with a
toolchain created by rules_python. Most notably, WORKSPACE builds default to
using a legacy toolchain built into Bazel itself which doesn't support the
script bootstrap. If not available, the `system_python` bootstrap will be used
instead.
:::

:::{seealso}
{obj}`PyRuntimeInfo.bootstrap_template` and
{obj}`PyRuntimeInfo.stage2_bootstrap_template`
:::

:::{versionadded} 0.33.0
:::

::::

::::{bzl:flag} current_config
Fail the build if the current build configuration does not match the
{obj}`pip.parse` defined wheels.

Values:
* `fail`: Will fail in the build action ensuring that we get the error
  message no matter the action cache.
* ``: (empty string) The default value, that will just print a warning.

:::{seealso}
{obj}`pip.parse`
:::

:::{versionadded} 1.1.0
:::

::::

::::{bzl:flag} venvs_use_declare_symlink

Determines if relative symlinks are created using `declare_symlink()` at build
time.

This is only intended to work around
[#2489](https://github.com/bazel-contrib/rules_python/issues/2489), where some
packaging rules don't support `declare_symlink()` artifacts.

Values:
* `yes`: Use `declare_symlink()` and create relative symlinks at build time.
* `no`: Do not use `declare_symlink()`. Instead, the venv will be created at
  runtime.

:::{seealso}
{envvar}`RULES_PYTHON_EXTRACT_ROOT` for customizing where the runtime venv
is created.
:::

:::{versionadded} 1.2.0
:::
::::
