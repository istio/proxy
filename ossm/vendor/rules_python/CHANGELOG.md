:::{default-domain} bzl
:::

# rules_python Changelog

This is a human-friendly changelog in a keepachangelog.com style format.
Because this changelog is for end-user consumption of meaningful changes, only
a summary of a release's changes is described. This means every commit is not
necessarily mentioned, and internal refactors or code cleanups are omitted
unless they're particularly notable.

A brief description of the categories of changes:

* `Changed`: Some behavior changed. If the change is expected to break a
  public API or supported behavior, it will be marked as **BREAKING**. Note that
  beta APIs will not have breaking API changes called out.
* `Fixed`: A bug, or otherwise incorrect behavior, was fixed.
* `Added`: A new feature, API, or behavior was added in a backwards compatible
  manner.
* Particular sub-systems are identified using parentheses, e.g. `(bzlmod)` or
  `(docs)`.

<!--
Unreleased changes template.

{#v0-0-0}
## Unreleased

[0.0.0]: https://github.com/bazelbuild/rules_python/releases/tag/0.0.0

{#v0-0-0-changed}
### Changed
* Nothing changed.

{#v0-0-0-fixed}
### Fixed
* Nothing fixed.

{#v0-0-0-added}
### Added
* Nothing added.

{#v0-0-0-removed}
### Removed
* Nothing removed.
-->

{#v0-0-0}
## Unreleased

[0.0.0]: https://github.com/bazelbuild/rules_python/releases/tag/0.0.0

{#v0-0-0-changed}
### Changed
* Nothing changed.

{#v0-0-0-fixed}
### Fixed
* Nothing fixed.

{#v0-0-0-added}
### Added
* Nothing added.

{#v0-0-0-removed}
### Removed
* Nothing removed.

{#v1-2-0}
## Unreleased

[1.2.0]: https://github.com/bazelbuild/rules_python/releases/tag/1.2.0

{#v1-2-0-changed}
### Changed
* (rules) `py_proto_library` is deprecated in favour of the
  implementation in https://github.com/protocolbuffers/protobuf. It will be
  removed in the future release.
* (pypi) {obj}`pip.override` will now be ignored instead of raising an error,
  fixes [#2550](https://github.com/bazelbuild/rules_python/issues/2550).
* (rules) deprecation warnings for deprecated symbols have been turned off by
  default for now and can be enabled with `RULES_PYTHON_DEPRECATION_WARNINGS`
  env var.
* (pypi) Downgraded versions of packages: `pip` from `24.3.2` to `24.0.0` and
  `packaging` from `24.2` to `24.0`.

{#v1-2-0-fixed}
### Fixed
* (rules) `python_zip_file` output with `--bootstrap_impl=script` works again
  ([#2596](https://github.com/bazelbuild/rules_python/issues/2596)).
* (docs) Using `python_version` attribute for specifying python versions introduced in `v1.1.0`
* (gazelle) Providing multiple input requirements files to `gazelle_python_manifest` now works correctly.
* (pypi) Handle trailing slashes in pip index URLs in environment variables,
  fixes [#2554](https://github.com/bazelbuild/rules_python/issues/2554).
* (runfiles) Runfile manifest and repository mapping files are now interpreted
  as UTF-8 on all platforms.
* (coverage) Coverage with `--bootstrap_impl=script` is fixed
  ([#2572](https://github.com/bazelbuild/rules_python/issues/2572)).
* (pypi) Non deterministic behaviour in requirement file usage has been fixed
  by reverting [#2514](https://github.com/bazelbuild/rules_python/pull/2514).
  The related issue is [#908](https://github.com/bazelbuild/rules_python/issue/908).
* (sphinxdocs) Do not crash when `tag_class` does not have a populated `doc` value.
  Fixes ([#2579](https://github.com/bazelbuild/rules_python/issues/2579)).
* (binaries/tests) Fix packaging when using `--bootstrap_impl=script`: set
  {obj}`--venvs_use_declare_symlink=no` to have it not create symlinks at
  build time (they will be created at runtime instead).
  (Fixes [#2489](https://github.com/bazelbuild/rules_python/issues/2489))

{#v1-2-0-added}
### Added
* Nothing added.

{#v1-2-0-removed}
### Removed
* Nothing removed.

{#v1-1-0}
## [1.1.0] - 2025-01-07

[1.1.0]: https://github.com/bazelbuild/rules_python/releases/tag/1.1.0

{#v1-1-0-changed}
### Changed
* (toolchains) 3.13 means 3.13.1 (previously 3.13.0)
* Bazel 6 support is dropped and Bazel 7.4.1 is the minimum supported
  version, per our Bazel support matrix. Earlier versions are not
  tested by CI, so functionality cannot be guaranteed.
* ({bzl:obj}`pip.parse`) From now we will make fewer calls to indexes when
  fetching the metadata from SimpleAPI. The calls will be done in parallel to
  each index separately, so the extension evaluation time might slow down if
  not using {bzl:obj}`pip.parse.experimental_index_url_overrides`.
* ({bzl:obj}`pip.parse`) Only query SimpleAPI for packages that have
  sha values in the `requirements.txt` file.
* (rules) The version-aware rules have been folded into the base rules and
  the version-aware rules are now simply aliases for the base rules. The
  `python_version` attribute is still used to specify the Python version.
* (pypi) Updated versions of packages: `pip` to 24.3.1 and
  `packaging` to 24.2.

{#v1-1-0-deprecations}
#### Deprecations
* `//python/config_settings:transitions.bzl` and its `py_binary` and `py_test`
  wrappers are deprecated. Use the regular rules instead.

{#v1-1-0-fixed}
### Fixed
* (py_wheel) Use the default shell environment when building wheels to allow
  toolchains that search PATH to be used for the wheel builder tool.
* (pypi) The requirement argument parsed to `whl_library` will now not have env
  marker information allowing `bazel query` to work in cases where the `whl` is
  available for all of the platforms and the sdist can be built. This fix is
  for both WORKSPACE and `bzlmod` setups.
  Fixes [#2450](https://github.com/bazelbuild/rules_python/issues/2450).
* (gazelle) Gazelle will now correctly parse Python3.12 files that use [PEP 695 Type
  Parameter Syntax][pep-695]. (#2396)
* (pypi) Using {bzl:obj}`pip_parse.experimental_requirement_cycles` and
  {bzl:obj}`pip_parse.use_hub_alias_dependencies` together now works when
  using WORKSPACE files.
* (pypi) The error messages when the wheel distributions do not match anything
  are now printing more details and include the currently active flag
  values. Fixes [#2466](https://github.com/bazelbuild/rules_python/issues/2466).
* (py_proto_library) Fix import paths in Bazel 8.
* (whl_library) Now the changes to the dependencies are correctly tracked when
  PyPI packages used in {bzl:obj}`whl_library` during the `repository_rule` phase
  change. Fixes [#2468](https://github.com/bazelbuild/rules_python/issues/2468).
+ (gazelle) Gazelle no longer ignores `setup.py` files by default. To restore
  this behavior, apply the `# gazelle:python_ignore_files setup.py` directive.
* Don't re-fetch whl_library, python_repository, etc. repository rules
  whenever `PATH` changes. Fixes
  [#2551](https://github.com/bazelbuild/rules_python/issues/2551).

[pep-695]: https://peps.python.org/pep-0695/

{#v1-1-0-added}
### Added
* (gazelle) Added `include_stub_packages`  flag to `modules_mapping`. When set to `True`, this
  automatically includes corresponding stub packages for third-party libraries
  that are present and used (e.g., `boto3` → `boto3-stubs`), improving
  type-checking support.
* (pypi) Freethreaded packages are now fully supported in the
  {obj}`experimental_index_url` usage or the regular `pip.parse` usage.
  To select the free-threaded interpreter in the repo phase, please use
  the documented [env](/environment-variables.html) variables.
  Fixes [#2386](https://github.com/bazelbuild/rules_python/issues/2386).
* (toolchains) Use the latest astrahl-sh toolchain release [20241206] for Python versions:
    * 3.9.21
    * 3.10.16
    * 3.11.11
    * 3.12.8
    * 3.13.1
* (rules) Attributes for type definition files (`.pyi` files) and type-checking
  only dependencies added. See {obj}`py_library.pyi_srcs` and
  `py_library.pyi_deps` (and the same named attributes for `py_binary` and
  `py_test`).
* (pypi) pypi-generated targets set `pyi_srcs` to include `*.pyi` files.
* (providers) {obj}`PyInfo` has new fields to aid static analysis tools:
  {obj}`direct_original_sources`, {obj}`direct_pyi_files`,
  {obj}`transitive_original_sources`, {obj}`transitive_pyi_files`.

[20241206]: https://github.com/astral-sh/python-build-standalone/releases/tag/20241206

{#v1-1-0-removed}
### Removed
* `find_requirements` in `//python:defs.bzl` has been removed.

{#v1-0-0}
## [1.0.0] - 2024-12-05

[1.0.0]: https://github.com/bazelbuild/rules_python/releases/tag/1.0.0

{#v1-0-0-changed}
### Changed

**Breaking**:
* (toolchains) stop exposing config settings in python toolchain alias repos.
  Please consider depending on the flags defined in
  `//python/config_setting/...` and the `@platforms` package instead.
* (toolchains) consumers who were depending on the `MACOS_NAME` and the `arch`
  attribute in the `PLATFORMS` list, please update your code to respect the new
  values. The values now correspond to the values available in the
  `@platforms//` package constraint values.
* (toolchains) `host_platform` and `interpreter` constants are no longer created
  in the `toolchain` generated alias `.bzl` files. If you need to access the
  host interpreter during the `repository_rule` evaluation, please use the
  `@python_{version}_host//:python` targets created by
  {bzl:obj}`python_register_toolchains` and
  {bzl:obj}`python_register_multi_toolchains` macros or the {bzl:obj}`python`
  bzlmod extension.
* (bzlmod) `pip.parse.parse_all_requirements_files` attribute has been removed.
  See notes in the previous versions about what to do.
* (deps) rules_cc 0.1.0 (workspace) and 0.0.16 (bzlmod).
* (deps) protobuf 29.0-rc2 (workspace; bzlmod already specifying that version).

Other changes:
* (python_repository) Start honoring the `strip_prefix` field for `zstd` archives.
* (pypi) {bzl:obj}`pip_parse.extra_hub_aliases` now works in WORKSPACE files.
* (binaries/tests) For {obj}`--bootstrap_impl=script`, a binary-specific (but
  otherwise empty) virtual env is used to customize `sys.path` initialization.
* (deps) bazel_skylib 1.7.0 (workspace; bzlmod already specifying that version)
* (deps) bazel_features 1.21.0; necessary for compatiblity with Bazel 8 rc3
* (deps) stardoc 0.7.2 to support Bazel 8.

{#v1-0-0-fixed}
### Fixed
* (toolchains) stop depending on `uname` to get the value of the host platform.
* (pypi): Correctly handle multiple versions of the same package in the requirements
  files which is useful when including different PyTorch builds (e.g. <pytorch+cpu> vs <pytorch+cu118> ) for different target platforms.
  Fixes ([2337](https://github.com/bazelbuild/rules_python/issues/2337)).
* (uv): Correct the sha256sum for the `uv` binary for aarch64-apple-darwin.
  Fixes ([2411](https://github.com/bazelbuild/rules_python/issues/2411)).
* (binaries/tests) ({obj}`--bootstrap_impl=scipt`) Using `sys.executable` will
  use the same `sys.path` setup as the calling binary.
  ([2169](https://github.com/bazelbuild/rules_python/issues/2169)).
* (workspace) Corrected protobuf's name to com_google_protobuf, the name is
  hardcoded in Bazel, WORKSPACE mode.
* (pypi): {bzl:obj}`compile_pip_requirements` no longer fails on Windows when `--enable_runfiles` is not enabled.
* (pypi): {bzl:obj}`compile_pip_requirements` now correctly updates files in the source tree on Windows when `--windows_enable_symlinks` is not enabled.
* (repositories): Add libs/python3.lib and pythonXY.dll to the `libpython` target
  defined by a repository template. This enables stable ABI builds of Python extensions
  on Windows (by defining Py_LIMITED_API).
* (rules) `py_test` and `py_binary` targets no longer incorrectly remove the
  first `sys.path` entry when using {obj}`--bootstrap_impl=script`

{#v1-0-0-added}
### Added
* (gazelle): Parser failures will now be logged to the terminal. Additional
  details can be logged by setting `RULES_PYTHON_GAZELLE_VERBOSE=1`.
* (toolchains) allow users to select which variant of the support host toolchain
  they would like to use through
  `RULES_PYTHON_REPO_TOOLCHAIN_{VERSION}_{OS}_{ARCH}` env variable setting. For
  example, this allows one to use `freethreaded` python interpreter in the
  `repository_rule` to build a wheel from `sdist`.
* (toolchain) The python interpreters targeting `muslc` libc have been added
  for the latest toolchain versions for each minor Python version. You can control
  the toolchain selection by using the
  {bzl:obj}`//python/config_settings:py_linux_libc` build flag.
* (providers) Added {obj}`py_runtime_info.site_init_template` and
  {obj}`PyRuntimeInfo.site_init_template` for specifying the template to use to
  initialize the interpreter via venv startup hooks.
* (runfiles) (Bazel 7.4+) Added support for spaces and newlines in runfiles paths

{#v1-0-0-removed}
### Removed
* (pypi): Remove `pypi_install_dependencies` macro that has been included in
  {bzl:obj}`py_repositories` for a long time.
* (bzlmod): Remove `DEFAULT_PYTHON_VERSION` from `interpreters.bzl` file. If
  you need the version, please use it from the `versions.bzl` file instead.

{#v0-40-0}
## [0.40.0] - 2024-11-17

[0.40.0]: https://github.com/bazelbuild/rules_python/releases/tag/0.40.0

{#v0-40-changed}
### Changed
* Nothing changed.

{#v0-40-fixed}
### Fixed
* (rules) Don't drop custom import paths if Bazel-builtin PyInfo is removed.
  ([2414](https://github.com/bazelbuild/rules_python/issues/2414)).

{#v0-40-added}
### Added
* Nothing added.

{#v0-40-removed}
### Removed
* (publish) Remove deprecated `requirements.txt` for the `twine` dependencies.
  Please use `requirements_linux.txt` instead.
* (python_repository) Use bazel's built in `zstd` support and remove attributes
  for customizing the `zstd` binary to be used for `zstd` archives in the
  {bzl:obj}`python_repository` repository_rule. This affects the
  {bzl:obj}`python_register_toolchains` and
  {bzl:obj}`python_register_multi_toolchains` callers in the `WORKSPACE`.

{#v0-39-0}
## [0.39.0] - 2024-11-13

[0.39.0]: https://github.com/bazelbuild/rules_python/releases/tag/0.39.0

{#v0-39-0-changed}
### Changed
* (deps) bazel_skylib 1.6.1 -> 1.7.1
* (deps) rules_cc 0.0.9 -> 0.0.14
* (deps) protobuf 24.4 -> 29.0-rc2
* (deps) rules_proto 6.0.0-rc1 -> 6.0.2
* (deps) stardoc 0.6.2 -> 0.7.1
* For bzlmod, Bazel 7.4 is now the minimum Bazel version.
* (toolchains) Use the latest indygreg toolchain release [20241016] for Python versions:
    * 3.9.20
    * 3.10.15
    * 3.11.10
    * 3.12.7
    * 3.13.0
* (pypi) The naming scheme for the `bzlmod` spoke repositories have changed as
  all of the given `requirements.txt` files are now parsed by `default`, to
  temporarily restore the behavior, you can use
  {bzl:obj}`pip.parse.extra_hub_aliases`, which will be removed or made noop in
  the future.

[20241016]: https://github.com/indygreg/python-build-standalone/releases/tag/20241016

{#v0-39-0-fixed}
### Fixed
* (precompiling) Skip precompiling (instead of erroring) if the legacy
  `@bazel_tools//tools/python:autodetecting_toolchain` is being used
  ([#2364](https://github.com/bazelbuild/rules_python/issues/2364)).

{#v0-39-0-added}
### Added
* Bazel 8 is now supported.
* (toolchain) Support for freethreaded Python toolchains is now available. Use
  the config flag `//python/config_settings:py_freethreaded` to toggle the
  selection of the free-threaded toolchains.
* (toolchain) {obj}`py_runtime.abi_flags` attribute and
  {obj}`PyRuntimeInfo.abi_flags` field added.

{#v0-39-0-removed}
### Removed
* Support for Bazel 6 using bzlmod has been dropped.

{#v0-38-0}
## [0.38.0] - 2024-11-08

[0.38.0]: https://github.com/bazelbuild/rules_python/releases/tag/0.38.0

{#v0-38-0-changed}
### Changed
* (deps) (WORKSPACE only) rules_cc 0.0.13 and protobuf 27.0 is now the default
  version used; this for Bazel 8+ support (previously version was rules_cc 0.0.9
  and no protobuf version specified)
  ([2310](https://github.com/bazelbuild/rules_python/issues/2310)).
* (publish) The dependencies have been updated to the latest available versions
  for the `twine` publishing rule.
* (whl_library) Remove `--no-build-isolation` to allow non-hermetic sdist builds
  by default. Users wishing to keep this argument and to enforce more hermetic
  builds can do so by passing the argument in
  [`pip.parse#extra_pip_args`](https://rules-python.readthedocs.io/en/latest/api/rules_python/python/extensions/pip.html#pip.parse.extra_pip_args)
* (pip.parse) {attr}`pip.parse.whl_modifications` now normalizes the given whl names
  and now `pyyaml` and `PyYAML` will both work.
* (bzlmod) `pip.parse` spoke repository naming will be changed in an upcoming
  release in places where the users specify different package versions per
  platform in the same hub repository. The naming of the spoke repos is
  considered an implementation detail and we advise the users to use the `hub`
  repository directly and make use of {bzl:obj}`pip.parse.extra_hub_aliases`
  feature added in this release.

{#v0-38-0-fixed}
### Fixed
* (pypi) (Bazel 7.4+) Allow spaces in filenames included in `whl_library`s
  ([617](https://github.com/bazelbuild/rules_python/issues/617)).
* (pypi) When {attr}`pip.parse.experimental_index_url` is set, we need to still
  pass the `extra_pip_args` value when building an `sdist`.
* (pypi) The patched wheel filenames from now on are using local version specifiers
  which fixes usage of the said wheels using standard package managers.
* (bzlmod) The extension evaluation has been adjusted to always generate the
  same lock file irrespective if `experimental_index_url` is set by any module
  or not. To opt into this behavior, set
  `pip.parse.parse_all_requirements_files`, which will become the
  default in future releases leading up to `1.0.0`. Fixes
  [#2268](https://github.com/bazelbuild/rules_python/issues/2268). A known
  issue is that it may break `bazel query` and in these use cases it is
  advisable to use `cquery` or switch to `download_only = True`

{#v0-38-0-added}
### Added
* (publish) The requirements file for the `twine` publishing rules have been
  updated to have a new convention: `requirements_darwin.txt`,
  `requirements_linux.txt`, `requirements_windows.txt` for each respective OS
  and one extra file `requirements_universal.txt` if you prefer a single file.
  The `requirements.txt` file may be removed in the future.
* The rules_python version is now reported in `//python/features.bzl#features.version`
* (pip.parse) {attr}`pip.parse.extra_hub_aliases` can now be used to expose extra
  targets created by annotations in whl repositories.
  Fixes [#2187](https://github.com/bazelbuild/rules_python/issues/2187).
* (bzlmod) `pip.parse` now supports `whl-only` setup using
  `download_only = True` where users can specify multiple requirements files
  and use the `pip` backend to do the downloading. This was only available for
  users setting {bzl:obj}`pip.parse.experimental_index_url`, but now users have
  more options whilst we continue to work on stabilizing the experimental feature.

{#v0-37-2}
## [0.37.2] - 2024-10-27

[0.37.2]: https://github.com/bazelbuild/rules_python/releases/tag/0.37.2

{#v0-37-2-fixed}
### Fixed
* (bzlmod) Generate `config_setting` values for all available toolchains instead
  of only the registered toolchains, which restores the previous behaviour that
  `bzlmod` users would have observed.

{#v0-37-1}
## [0.37.1] - 2024-10-22

[0.37.1]: https://github.com/bazelbuild/rules_python/releases/tag/0.37.1

{#v0-37-1-fixed}
### Fixed
* (rules) Setting `--incompatible_python_disallow_native_rules` no longer
  causes rules_python rules to fail
  ([#2326](https://github.com/bazelbuild/rules_python/issues/2326)).

{#v0-37-0}
## [0.37.0] - 2024-10-18

[0.37.0]: https://github.com/bazelbuild/rules_python/releases/tag/0.37.0

{#v0-37-0-changed}
### Changed
* **BREAKING** `py_library` no longer puts its source files or generated pyc
  files in runfiles; it's the responsibility of consumers (e.g. binaries) to
  populate runfiles with the necessary files. Adding source files to runfiles
  can be temporarily restored by setting {obj}`--add_srcs_to_runfiles=enabled`,
  but this flag will be removed in a subsequent releases.
* {obj}`PyInfo.transitive_sources` is now added to runfiles. These files are
  `.py` files that are required to be added to runfiles by downstream binaries
  (or equivalent).
* (toolchains) `py_runtime.implementation_name` now defaults to `cpython`
  (previously it defaulted to None).
* (toolchains) The exec tools toolchain is enabled by default. It can be
  disabled by setting
  {obj}`--@rules_python//python/config_settings:exec_tools_toolchain=disabled`.
* (deps) stardoc 0.6.2 added as dependency.

{#v0-37-0-fixed}
### Fixed
* (bzlmod) The `python.override(minor_mapping)` now merges the default and the
  overridden versions ensuring that the resultant `minor_mapping` will always
  have all of the python versions.
* (bzlmod) The default value for the {obj}`--python_version` flag will now be
  always set to the default python toolchain version value.
* (bzlmod) correctly wire the {attr}`pip.parse.extra_pip_args` all the
  way to {obj}`whl_library`. What is more we will pass the `extra_pip_args` to
  {obj}`whl_library` for `sdist` distributions when using
  {attr}`pip.parse.experimental_index_url`. See
  [#2239](https://github.com/bazelbuild/rules_python/issues/2239).
* (whl_filegroup): Provide per default also the `RECORD` file
* (py_wheel): `RECORD` file entry elements are now quoted if necessary when a
  wheel is created
* (whl_library) truncate progress messages from the repo rule to better handle
  case where a requirement has many `--hash=sha256:...` flags
* (rules) `compile_pip_requirements` passes `env` to the `X.update` target (and
  not only to the `X_test` target, a bug introduced in
  [#1067](https://github.com/bazelbuild/rules_python/pull/1067)).
* (bzlmod) In hybrid bzlmod with WORKSPACE builds,
  `python_register_toolchains(register_toolchains=True)` is respected
  ([#1675](https://github.com/bazelbuild/rules_python/issues/1675)).
* (precompiling) The {obj}`pyc_collection` attribute now correctly
  enables (or disables) using pyc files from targets transitively
* (pip) Skip patching wheels not matching `pip.override`'s `file`
  ([#2294](https://github.com/bazelbuild/rules_python/pull/2294)).
* (chore): Add a `rules_shell` dev dependency and moved a `sh_test` target
  outside of the `//:BUILD.bazel` file.
  Fixes [#2299](https://github.com/bazelbuild/rules_python/issues/2299).

{#v0-37-0-added}
### Added
* (py_wheel) Now supports `compress = (True|False)` to allow disabling
  compression to speed up development.
* (toolchains): A public `//python/config_settings:python_version_major_minor` has
  been exposed for users to be able to match on the `X.Y` version of a Python
  interpreter.
* (api) Added {obj}`merge_py_infos()` so user rules can merge and propagate
  `PyInfo` without losing information.
* (toolchains) New Python versions available: 3.13.0 using the [20241008] release.
* (toolchains): Bump default toolchain versions to:
    * `3.8 -> 3.8.20`
    * `3.9 -> 3.9.20`
    * `3.10 -> 3.10.15`
    * `3.11 -> 3.11.10`
    * `3.12 -> 3.12.7`
* (coverage) Add support for python 3.13 and bump `coverage.py` to 7.6.1.
* (bzlmod) Add support for `download_only` flag to disable usage of `sdists`
  when {bzl:attr}`pip.parse.experimental_index_url` is set.
* (api) PyInfo fields: {obj}`PyInfo.transitive_implicit_pyc_files`,
  {obj}`PyInfo.transitive_implicit_pyc_source_files`.

[20241008]: https://github.com/indygreg/python-build-standalone/releases/tag/20241008

{#v0-37-0-removed}
### Removed
* (precompiling) {obj}`--precompile_add_to_runfiles` has been removed.
* (precompiling) {obj}`--pyc_collection` has been removed. The `pyc_collection`
  attribute now bases its default on {obj}`--precompile`.
* (precompiling) The {obj}`precompile=if_generated_source` value has been removed.
* (precompiling) The {obj}`precompile_source_retention=omit_if_generated_source` value has been removed.

{#v0-36-0}
## [0.36.0] - 2024-09-24

[0.36.0]: https://github.com/bazelbuild/rules_python/releases/tag/0.36.0

{#v0-36-0-changed}
### Changed
* (gazelle): Update error messages when unable to resolve a dependency to be more human-friendly.
* (flags) The {obj}`--python_version` flag now also returns
  {obj}`config_common.FeatureFlagInfo`.
* (toolchain): The toolchain patches now expose the `patch_strip` attribute
  that one should use when patching toolchains. Please set it if you are
  patching python interpreter. In the next release the default will be set to
  `0` which better reflects the defaults used in public `bazel` APIs.
* (toolchains) When {obj}`py_runtime.interpreter_version_info` isn't specified,
  the {obj}`--python_version` flag will determine the value. This allows
  specifying the build-time Python version for the
  {obj}`runtime_env_toolchains`.
* (toolchains) {obj}`py_cc_toolchain.libs` and {obj}`PyCcToolchainInfo.libs` is
  optional. This is to support situations where only the Python headers are
  available.
* (bazel) Minimum bazel 7 version that we test against has been bumped to `7.1`.

{#v0-36-0-fixed}
### Fixed
* (whl_library): Remove `--no-index` and add `--no-build-isolation` to the
  `pip install` command when installing a wheel from a local file, which happens
  when `experimental_index_url` flag is used.
* (bzlmod) get the path to the host python interpreter in a way that results in
  platform non-dependent hashes in the lock file when the requirement markers need
  to be evaluated.
* (bzlmod) correctly watch sources used for evaluating requirement markers for
  any changes so that the repository rule or module extensions can be
  re-evaluated when the said files change.
* (gazelle): Fix incorrect use of `t.Fatal`/`t.Fatalf` in tests.
* (toolchain) Omit third-party python packages from coverage reports from
  stage2 bootstrap template.
* (bzlmod) Properly handle relative path URLs in parse_simpleapi_html.bzl
* (gazelle) Correctly resolve deps that have top-level module overlap with a gazelle_python.yaml dep module
* (rules) Make `RUNFILES_MANIFEST_FILE`-based invocations work when used with
  {obj}`--bootstrap_impl=script`. This fixes invocations using non-sandboxed
  test execution with `--enable_runfiles=false --build_runfile_manifests=true`.
  ([#2186](https://github.com/bazelbuild/rules_python/issues/2186)).
* (py_wheel) Fix incorrectly generated `Required-Dist` when specifying requirements with markers
  in extra_requires in py_wheel rule.
* (rules) Prevent pytest from trying run the generated stage2
  bootstrap .py file when using {obj}`--bootstrap_impl=script`
* (toolchain) The {bzl:obj}`gen_python_config_settings` has been fixed to include
  the flag_values from the platform definitions.

{#v0-36-0-added}
### Added
* (bzlmod): Toolchain overrides can now be done using the new
  {bzl:obj}`python.override`, {bzl:obj}`python.single_version_override` and
  {bzl:obj}`python.single_version_platform_override` tag classes.
  See [#2081](https://github.com/bazelbuild/rules_python/issues/2081).
* (rules) Executables provide {obj}`PyExecutableInfo`, which contains
  executable-specific information useful for packaging an executable or
  or deriving a new one from the original.
* (py_wheel) Removed use of bash to avoid failures on Windows machines which do not
  have it installed.
* (docs) Automatically generated documentation for {bzl:obj}`python_register_toolchains`
  and related symbols.
* (toolchains) Added {attr}`python_repository.patch_strip` attribute for
  allowing values that are other than `1`, which has been hard-coded up until
  now. If you are relying on the undocumented `patches` support in
  `TOOL_VERSIONS` for registering patched toolchains please consider setting
  the `patch_strip` explicitly to `1` if you depend on this value - in the
  future the value may change to default to `0`.
* (toolchains) Added `//python:none`, a special target for use with
  {obj}`py_exec_tools_toolchain.exec_interpreter` to treat the value as `None`.

{#v0-36-0-removed}
### Removed
* (toolchains): Removed accidentally exposed `http_archive` symbol from
  `python/repositories.bzl`.
* (toolchains): An internal _is_python_config_setting_ macro has been removed.

{#v0-35-0}
## [0.35.0] - 2024-08-15

[0.35.0]: https://github.com/bazelbuild/rules_python/releases/tag/0.35.0

{#v0-35-0-changed}
### Changed
* (whl_library) A better log message when the wheel is built from an sdist or
  when the wheel is downloaded using `download_only` feature to aid debugging.
* (gazelle): Simplify and make gazelle_python.yaml have only top level package name.
  It would work well in cases to reduce merge conflicts.
* (toolchains): Change some old toochain versions to use [20240726] release to
  include dependency updates `3.8.19`, `3.9.19`, `3.10.14`, `3.11.9`
* (toolchains): Bump default toolchain versions to:
    * `3.12 -> 3.12.4`
* (rules) `PYTHONSAFEPATH` is inherited from the calling environment to allow
  disabling it (Requires {obj}`--bootstrap_impl=script`)
  ([#2060](https://github.com/bazelbuild/rules_python/issues/2060)).

{#v0-35-0-fixed}
### Fixed
* (rules) `compile_pip_requirements` now sets the `USERPROFILE` env variable on
  Windows to work around an issue where `setuptools` fails to locate the user's
  home directory.
* (rules) correctly handle absolute URLs in parse_simpleapi_html.bzl.
* (rules) Fixes build targets linking against `@rules_python//python/cc:current_py_cc_libs`
  in host platform builds on macOS, by editing the `LC_ID_DYLIB` field of the hermetic interpreter's
  `libpython3.x.dylib` using `install_name_tool`, setting it to its absolute path under Bazel's
  execroot.
* (rules) Signals are properly received when using {obj}`--bootstrap_impl=script`
  (for non-zip builds).
  ([#2043](https://github.com/bazelbuild/rules_python/issues/2043))
* (rules) Fixes Python builds when the `--build_python_zip` is set to `false` on
  Windows. See [#1840](https://github.com/bazelbuild/rules_python/issues/1840).
* (rules) Fixes Mac + `--build_python_zip` + {obj}`--bootstrap_impl=script`
  ([#2030](https://github.com/bazelbuild/rules_python/issues/2030)).
* (rules) User dependencies come before runtime site-packages when using
  {obj}`--bootstrap_impl=script`.
  ([#2064](https://github.com/bazelbuild/rules_python/issues/2064)).
* (rules) Version-aware rules now return both `@_builtins` and `@rules_python`
  providers instead of only one.
  ([#2114](https://github.com/bazelbuild/rules_python/issues/2114)).
* (pip) Fixed pypi parse_simpleapi_html function for feeds with package metadata
  containing ">" sign
* (toolchains) Added missing executable permission to
  `//python/runtime_env_toolchains` interpreter script so that it is runnable.
  ([#2085](https://github.com/bazelbuild/rules_python/issues/2085)).
* (pip) Correctly use the `sdist` downloaded by the bazel downloader when using
  `experimental_index_url` feature. Fixes
  [#2091](https://github.com/bazelbuild/rules_python/issues/2090).
* (gazelle) Make `gazelle_python_manifest.update` manual to avoid unnecessary
  network behavior.
* (bzlmod): The conflicting toolchains during `python` extension will no longer
  cause warnings by default. In order to see the warnings for diagnostic purposes
  set the env var `RULES_PYTHON_REPO_DEBUG_VERBOSITY` to one of `INFO`, `DEBUG` or `TRACE`.
  Fixes [#1818](https://github.com/bazelbuild/rules_python/issues/1818).
* (runfiles) Make runfiles lookups work for the situation of Bazel 7,
  Python 3.9 (or earlier, where safepath isn't present), and the Rlocation call
  in the same directory as the main file.
  Fixes [#1631](https://github.com/bazelbuild/rules_python/issues/1631).

{#v0-35-0-added}
### Added
* (rules) `compile_pip_requirements` supports multiple requirements input files as `srcs`.
* (rules) `PYTHONSAFEPATH` is inherited from the calling environment to allow
  disabling it (Requires {obj}`--bootstrap_impl=script`)
  ([#2060](https://github.com/bazelbuild/rules_python/issues/2060)).
* (gazelle) Added `python_generation_mode_per_package_require_test_entry_point`
  in order to better accommodate users who use a custom macro,
  [`pytest-bazel`][pytest_bazel], [rules_python_pytest] or `rules_py`
  [py_test_main] in order to integrate with `pytest`. Currently the default
  flag value is set to `true` for backwards compatible behaviour, but in the
  future the flag will be flipped be `false` by default.
* (toolchains) New Python versions available: `3.12.4` using the [20240726] release.
* (pypi) Support env markers in requirements files. Note, that this means that
  if your requirements files contain env markers, the Python interpreter will
  need to be run during bzlmod phase to evaluate them. This may incur
  downloading an interpreter (for hermetic-based builds) or cause non-hermetic
  behavior (if using a system Python).

[rules_python_pytest]: https://github.com/caseyduquettesc/rules_python_pytest
[py_test_main]: https://docs.aspect.build/rulesets/aspect_rules_py/docs/rules/#py_pytest_main
[pytest_bazel]: https://pypi.org/project/pytest-bazel
[20240726]: https://github.com/indygreg/python-build-standalone/releases/tag/20240726

{#v0-34-0}
## [0.34.0] - 2024-07-04

[0.34.0]: https://github.com/bazelbuild/rules_python/releases/tag/0.34.0

{#v0-34-0-changed}
### Changed
* `protobuf`/`com_google_protobuf` dependency bumped to `v24.4`
* (bzlmod): optimize the creation of config settings used in pip to
  reduce the total number of targets in the hub repo.
* (toolchains) The exec tools toolchain now finds its interpreter by reusing
  the regular interpreter toolchain. This avoids having to duplicate specifying
  where the runtime for the exec tools toolchain is.
* (toolchains) ({obj}`//python:autodetecting_toolchain`) is deprecated. It is
  replaced by {obj}`//python/runtime_env_toolchains:all`. The old target will be
  removed in a future release.

{#v0-34-0-fixed}
### Fixed
* (bzlmod): When using `experimental_index_url` the `all_requirements`,
  `all_whl_requirements` and `all_data_requirements` will now only include
  common packages that are available on all target platforms. This is to ensure
  that packages that are only present for some platforms are pulled only via
  the `deps` of the materialized `py_library`. If you would like to include
  platform specific packages, using a `select` statement with references to the
  specific package will still work (e.g.
  ```
  my_attr = all_requirements + select(
      {
          "@platforms//os:linux": ["@pypi//foo_available_only_on_linux"],
          "//conditions:default": [],
      }
  )
  ```
* (bzlmod): Targets in `all_requirements` now use the same form as targets returned by the `requirement` macro.
* (rules) Auto exec groups are enabled. This allows actions run by the rules,
  such as precompiling, to pick an execution platform separately from what
  other toolchains support.
* (providers) {obj}`PyRuntimeInfo` doesn't require passing the
  `interpreter_version_info` arg.
* (bzlmod) Correctly pass `isolated`, `quiet` and `timeout` values to `whl_library`
  and drop the defaults from the lock file.
* (whl_library) Correctly handle arch-specific dependencies when we encounter a
  platform specific wheel and use `experimental_target_platforms`.
  Fixes [#1996](https://github.com/bazelbuild/rules_python/issues/1996).
* (rules) The first element of the default outputs is now the executable again.
* (pip) Fixed crash when pypi packages lacked a sha (e.g. yanked packages)

{#v0-34-0-added}
### Added
* (toolchains) {obj}`//python/runtime_env_toolchains:all`, which is a drop-in
  replacement for the "autodetecting" toolchain.
* (gazelle) Added new `python_label_convention` and `python_label_normalization` directives. These directive
  allows altering default Gazelle label format to third-party dependencies useful for re-using Gazelle plugin
  with other rules, including `rules_pycross`. See [#1939](https://github.com/bazelbuild/rules_python/issues/1939).

{#v0-34-0-removed}
### Removed
* (pip): Removes the `entrypoint` macro that was replaced by `py_console_script_binary` in 0.26.0.

{#v0-33-2}
## [0.33.2] - 2024-06-13

[0.33.2]: https://github.com/bazelbuild/rules_python/releases/tag/0.33.2

{#v0-33-2-fixed}
### Fixed
* (toolchains) The {obj}`exec_tools_toolchain_type` is disabled by default.
  To enable it, set {obj}`--//python/config_settings:exec_tools_toolchain=enabled`.
  This toolchain must be enabled for precompilation to work. This toolchain will
  be enabled by default in a future release.
  Fixes [#1967](https://github.com/bazelbuild/rules_python/issues/1967).

{#v0-33-1}
## [0.33.1] - 2024-06-13

[0.33.1]: https://github.com/bazelbuild/rules_python/releases/tag/0.33.1

{#v0-33-1-fixed}
### Fixed
* (py_binary) Fix building of zip file when using `--build_python_zip`
  argument. Fixes [#1954](https://github.com/bazelbuild/rules_python/issues/1954).

{#v0-33-0}
## [0.33.0] - 2024-06-12

[0.33.0]: https://github.com/bazelbuild/rules_python/releases/tag/0.33.0

{#v0-33-0-changed}
### Changed
* (deps) Upgrade the `pip_install` dependencies to pick up a new version of pip.
* (toolchains) Optional toolchain dependency: `py_binary`, `py_test`, and
  `py_library` now depend on the `//python:exec_tools_toolchain_type` for build
  tools.
* (deps): Bumped `bazel_skylib` to 1.6.1.
* (bzlmod): The `python` and internal `rules_python` extensions have been
  marked as `reproducible` and will not include any lock file entries from now
  on.
* (gazelle): Remove gazelle plugin's python deps and make it hermetic.
  Introduced a new Go-based helper leveraging tree-sitter for syntax analysis.
  Implemented the use of `pypi/stdlib-list` for standard library module verification.
* (pip.parse): Do not ignore yanked packages when using `experimental_index_url`.
  This is to mimic what `uv` is doing. We will print a warning instead.
* (pip.parse): Add references to all supported wheels when using `experimental_index_url`
  to allowing to correctly fetch the wheels for the right platform. See the
  updated docs on how to use the feature. This is work towards addressing
  [#735](https://github.com/bazelbuild/rules_python/issues/735) and
  [#260](https://github.com/bazelbuild/rules_python/issues/260). The spoke
  repository names when using this flag will have a structure of
  `{pip_hub_prefix}_{wheel_name}_{py_tag}_{abi_tag}_{platform_tag}_{sha256}`,
  which is an implementation detail which should not be relied on and is there
  purely for better debugging experience.
* (bzlmod) The `pythons_hub//:interpreters.bzl` no longer has platform-specific
  labels which where left there for compatibility reasons. Move to
  `python_{version}_host` keys if you would like to have access to a Python
  interpreter that can be used in a repository rule context.

{#v0-33-0-fixed}
### Fixed
* (gazelle) Remove `visibility` from `NonEmptyAttr`.
  Now empty(have no `deps/main/srcs/imports` attr) `py_library/test/binary` rules will
  be automatically deleted correctly. For example, if `python_generation_mode`
  is set to package, when `__init__.py` is deleted, the `py_library` generated
  for this package before will be deleted automatically.
* (whl_library): Use _is_python_config_setting_ to correctly handle multi-python
  version dependency select statements when the `experimental_target_platforms`
  includes the Python ABI. The default python version case within the select is
  also now handled correctly, stabilizing the implementation.
* (gazelle) Fix Gazelle failing on Windows with
  "panic: runtime error: invalid memory address or nil pointer dereference"
* (bzlmod) remove `pip.parse(annotations)` attribute as it is unused and has been
  replaced by whl_modifications.
* (pip) Correctly select wheels when the python tag includes minor versions.
  See ([#1930](https://github.com/bazelbuild/rules_python/issues/1930))
* (pip.parse): The lock file is now reproducible on any host platform if the
  `experimental_index_url` is not used by any of the modules in the dependency
  chain. To make the lock file identical on each `os` and `arch`, please use
  the `experimental_index_url` feature which will fetch metadata from PyPI or a
  different private index and write the contents to the lock file. Fixes
  [#1643](https://github.com/bazelbuild/rules_python/issues/1643).
* (pip.parse): Install `yanked` packages and print a warning instead of
  ignoring them. This better matches the behaviour of `uv pip install`.
* (toolchains): Now matching of the default hermetic toolchain is more robust
  and explicit and should fix rare edge-cases where the host toolchain
  autodetection would match a different toolchain than expected. This may yield
  to toolchain selection failures when the python toolchain is not registered,
  but is requested via `//python/config_settings:python_version` flag setting.
* (doc) Fix the `WORKSPACE` requirement vendoring example. Fixes
  [#1918](https://github.com/bazelbuild/rules_python/issues/1918).

{#v0-33-0-added}
### Added
* (rules) Precompiling Python source at build time is available. but is
  disabled by default, for now. Set
  `@rules_python//python/config_settings:precompile=enabled` to enable it
  by default. A subsequent release will enable it by default. See the
  [Precompiling docs][precompile-docs] and API reference docs for more
  information on precompiling. Note this requires Bazel 7+ and the Pystar rule
  implementation enabled.
  ([#1761](https://github.com/bazelbuild/rules_python/issues/1761))
* (rules) Attributes and flags to control precompile behavior: `precompile`,
  `precompile_optimize_level`, `precompile_source_retention`,
  `precompile_invalidation_mode`, and `pyc_collection`
* (toolchains) The target runtime toolchain (`//python:toolchain_type`) has
  two new optional attributes: `pyc_tag` (tells the pyc filename infix to use) and
  `implementation_name` (tells the Python implementation name).
* (toolchains) A toolchain type for build tools has been added:
  `//python:exec_tools_toolchain_type`.
* (providers) `PyInfo` has two new attributes: `direct_pyc_files` and
  `transitive_pyc_files`, which tell the pyc files a target makes available
  directly and transitively, respectively.
* `//python:features.bzl` added to allow easy feature-detection in the future.
* (pip) Allow specifying the requirements by (os, arch) and add extra
  validations when parsing the inputs. This is a non-breaking change for most
  users unless they have been passing multiple `requirements_*` files together
  with `extra_pip_args = ["--platform=manylinux_2_4_x86_64"]`, that was an
  invalid usage previously but we were not failing the build. From now on this
  is explicitly disallowed.
* (toolchains) Added riscv64 platform definition for python toolchains.
* (gazelle) The `python_visibility` directive now supports the `$python_root$`
  placeholder, just like the `python_default_visibility` directive does.
* (rules) A new bootstrap implementation that doesn't require a system Python
  is available. It can be enabled by setting
  {obj}`--@rules_python//python/config_settings:bootstrap_impl=script`. It
  will become the default in a subsequent release.
  ([#691](https://github.com/bazelbuild/rules_python/issues/691))
* (providers) `PyRuntimeInfo` has two new attributes:
  {obj}`PyRuntimeInfo.stage2_bootstrap_template` and
  {obj}`PyRuntimeInfo.zip_main_template`.
* (toolchains) A replacement for the Bazel-builtn autodetecting toolchain is
  available. The `//python:autodetecting_toolchain` alias now uses it.
* (pip): Support fetching and using the wheels for other platforms. This
  supports customizing whether the linux wheels are pulled for `musl` or
  `glibc`, whether `universal2` or arch-specific MacOS wheels are preferred and
  it also allows to select a particular `libc` version. All of this is done via
  the `string_flags` in `@rules_python//python/config_settings`. If there are
  no wheels that are supported for the target platform, `rules_python` will
  fallback onto building the `sdist` from source. This behaviour can be
  disabled if desired using one of the available string flags as well.
* (whl_filegroup) Added a new `whl_filegroup` rule to extract files from a wheel file.
  This is useful to extract headers for use in a `cc_library`.

[precompile-docs]: /precompiling

{#v0-32-2}
## [0.32.2] - 2024-05-14

[0.32.2]: https://github.com/bazelbuild/rules_python/releases/tag/0.32.2

{#v0-32-2-fixed}
### Fixed

* Workaround existence of infinite symlink loops on case insensitive filesystems when targeting linux platforms with recent Python toolchains. Works around an upstream [issue][indygreg-231]. Fixes [#1800][rules_python_1800].

[indygreg-231]: https://github.com/indygreg/python-build-standalone/issues/231
[rules_python_1800]: https://github.com/bazelbuild/rules_python/issues/1800

{#v0-32-0}
## [0.32.0] - 2024-05-12

[0.32.0]: https://github.com/bazelbuild/rules_python/releases/tag/0.32.0

{#v0-32-0-changed}
### Changed

* (bzlmod): The `MODULE.bazel.lock` `whl_library` rule attributes are now
  sorted in the attributes section. We are also removing values that are not
  default in order to reduce the size of the lock file.
* (coverage) Bump `coverage.py` to [7.4.3](https://github.com/nedbat/coveragepy/blob/master/CHANGES.rst#version-743--2024-02-23).
* (deps): Bumped `bazel_features` to 1.9.1 to detect optional support
  non-blocking downloads.
* (deps): Updated `pip_tools` to >= 7.4.0
* (toolchains): Change some old toolchain versions to use [20240224] release to
  include security fixes `3.8.18`, `3.9.18` and `3.10.13`
* (toolchains): Bump default toolchain versions to:
    * `3.8 -> 3.8.19`
    * `3.9 -> 3.9.19`
    * `3.10 -> 3.10.14`
    * `3.11 -> 3.11.9`
    * `3.12 -> 3.12.3`

### Fixed

* (whl_library): Fix the experimental_target_platforms overriding for platform
  specific wheels when the wheels are for any python interpreter version. Fixes
  [#1810](https://github.com/bazelbuild/rules_python/issues/1810).
* (whl_library): Stop generating duplicate dependencies when encountering
  duplicates in the METADATA. Fixes
  [#1873](https://github.com/bazelbuild/rules_python/issues/1873).
* (gazelle) In `project` or `package` generation modes, do not generate `py_test`
  rules when there are no test files and do not set `main = "__test__.py"` when
  that file doesn't exist.
* (whl_library) The group redirection is only added when the package is part of
  the group potentially fixing aspects that want to traverse a `py_library` graph.
  Fixes [#1760](https://github.com/bazelbuild/rules_python/issues/1760).
* (bzlmod) Setting a particular micro version for the interpreter and the
  `pip.parse` extension is now possible, see the
  `examples/pip_parse/MODULE.bazel` for how to do it.
  See [#1371](https://github.com/bazelbuild/rules_python/issues/1371).
* (refactor) The pre-commit developer workflow should now pass `isort` and `black`
  checks (see [#1674](https://github.com/bazelbuild/rules_python/issues/1674)).

### Added

* (toolchains) Added armv7 platform definition for python toolchains.
* (toolchains) New Python versions available: `3.11.8`, `3.12.2` using the [20240224] release.
* (toolchains) New Python versions available: `3.8.19`, `3.9.19`, `3.10.14`, `3.11.9`, `3.12.3` using
  the [20240415] release.
* (gazelle) Added a new `python_visibility` directive to control visibility
  of generated targets by appending additional visibility labels.
* (gazelle) Added a new `python_default_visibility` directive to control the
  _default_ visibility of generated targets. See the [docs][python_default_visibility]
  for details.
* (gazelle) Added a new `python_test_file_pattern` directive. This directive tells
  gazelle which python files should be mapped to the `py_test` rule. See the
  [original issue][test_file_pattern_issue] and the [docs][test_file_pattern_docs]
  for details.
* (wheel) Add support for `data_files` attributes in py_wheel rule
  ([#1777](https://github.com/bazelbuild/rules_python/issues/1777))
* (py_wheel) `bzlmod` installations now provide a `twine` setup for the default
  Python toolchain in `rules_python` for version 3.11.
* (bzlmod) New `experimental_index_url`, `experimental_extra_index_urls` and
  `experimental_index_url_overrides` to `pip.parse` for using the bazel
  downloader. If you see any issues, report in
  [#1357](https://github.com/bazelbuild/rules_python/issues/1357). The URLs for
  the whl and sdist files will be written to the lock file. Controlling whether
  the downloading of metadata is done in parallel can be done using
  `parallel_download` attribute.
* (gazelle) Add a new annotation `include_dep`. Also add documentation for
  annotations to `gazelle/README.md`.
* (deps): `rules_python` depends now on `rules_cc` 0.0.9
* (pip_parse): A new flag `use_hub_alias_dependencies` has been added that is going
  to become default in the next release. This makes use of `dep_template` flag
  in the `whl_library` rule. This also affects the
  `experimental_requirement_cycles` feature where the dependencies that are in
  a group would be only accessible via the hub repo aliases. If you still
  depend on legacy labels instead of the hub repo aliases and you use the
  `experimental_requirement_cycles`, now is a good time to migrate.

[python_default_visibility]: gazelle/README.md#directive-python_default_visibility
[test_file_pattern_issue]: https://github.com/bazelbuild/rules_python/issues/1816
[test_file_pattern_docs]: gazelle/README.md#directive-python_test_file_pattern
[20240224]: https://github.com/indygreg/python-build-standalone/releases/tag/20240224.
[20240415]: https://github.com/indygreg/python-build-standalone/releases/tag/20240415.


## [0.31.0] - 2024-02-12

[0.31.0]: https://github.com/bazelbuild/rules_python/releases/tag/0.31.0

### Changed

* For Bazel 7, the core rules and providers are now implemented in rules_python
  directly and the rules bundled with Bazel are not used. Bazel 6 and earlier
  continue to use the Bazel builtin symbols. Of particular note, this means,
  under Bazel 7, the builtin global symbol `PyInfo` is **not** the same as what
  is loaded from rules_python. The same is true of `PyRuntimeInfo`.

## [0.30.0] - 2024-02-12

[0.30.0]: https://github.com/bazelbuild/rules_python/releases/tag/0.30.0

### Changed

* (toolchains) Windows hosts always ignore pyc files in the downloaded runtimes.
  This fixes issues due to pyc files being created at runtime and affecting the
  definition of what files were considered part of the runtime.

* (pip_parse) Added the `envsubst` parameter, which enables environment variable
  substitutions in the `extra_pip_args` attribute.

* (pip_repository) Added the `envsubst` parameter, which enables environment
  variable substitutions in the `extra_pip_args` attribute.

### Fixed

* (bzlmod) pip.parse now does not fail with an empty `requirements.txt`.

* (py_wheel) Wheels generated by `py_wheel` now preserve executable bits when
  being extracted by `installer` and/or `pip`.

* (coverage) During the running of lcov, the stdout/stderr was causing test
  failures.  By default, suppress output when generating lcov.  This can be
  overridden by setting 'VERBOSE_COVERAGE'.  This change only affect bazel
  7.x.x and above.

* (toolchain) Changed the `host_toolchain` to symlink all files to support
  Windows host environments without symlink support.

* (PyRuntimeInfo) Switch back to builtin PyRuntimeInfo for Bazel 6.4 and when
  pystar is disabled. This fixes an error about `target ... does not have ...
  PyRuntimeInfo`.
  ([#1732](https://github.com/bazelbuild/rules_python/issues/1732))

### Added

* (py_wheel) Added `requires_file` and `extra_requires_files` attributes.

* (whl_library) *experimental_target_platforms* now supports specifying the
  Python version explicitly and the output `BUILD.bazel` file will be correct
  irrespective of the python interpreter that is generating the file and
  extracting the `whl` distribution. Multiple python target version can be
  specified and the code generation will generate version specific dependency
  closures but that is not yet ready to be used and may break the build if
  the default python version is not selected using
  `common --@rules_python//python/config_settings:python_version=X.Y.Z`.

* New Python versions available: `3.11.7`, `3.12.1` using
  https://github.com/indygreg/python-build-standalone/releases/tag/20240107.

* (toolchain) Allow setting `x.y` as the `python_version` parameter in
  the version-aware `py_binary` and `py_test` rules. This allows users to
  use the same rule import for testing with specific Python versions and
  rely on toolchain configuration and how the latest version takes precedence
  if e.g. `3.8` is selected. That also simplifies `.bazelrc` for any users
  that set the default `python_version` string flag in that way.

* (toolchain) The runtime's shared libraries (libpython.so et al) can be
  accessed using `@rules_python//python/cc:current_py_cc_libs`. This uses
  toolchain resolution, so the files are from the same runtime used to run a
  target. If you were previously using e.g. `@python_3_11//:libpython`, then
  switch to `:current_py_cc_libs` for looser coupling to the underlying runtime
  repo implementation.

* (repo rules) The environment variable `RULES_PYTHON_REPO_DEBUG=1` can be
  set to make repository rules log detailed information about what they're
  up to.

* (coverage) Add support for python 3.12 and bump `coverage.py` to
  7.4.1.


## [0.29.0] - 2024-01-22

[0.29.0]: https://github.com/bazelbuild/rules_python/releases/tag/0.29.0

### Changed

* **BREAKING** The deprecated `incompatible_generate_aliases` feature flags
  from `pip_parse` and `gazelle` got removed. They had been flipped to `True`
  in 0.27.0 release.
* **BREAKING** (wheel) The `incompatible_normalize_name` and
  `incompatible_normalize_version` flags have been removed. They had been
  flipped to `True` in 0.27.0 release.
* (bzlmod) The pip hub repository now uses the newly introduced config settings
  using the `X.Y` python version notation. This improves cross module
  interoperability and allows to share wheels built by interpreters using
  different patch versions.

### Fixed

* (bzlmod pip.parse) Use a platform-independent reference to the interpreter
  pip uses. This reduces (but doesn't eliminate) the amount of
  platform-specific content in `MODULE.bazel.lock` files; Follow
  [#1643](https://github.com/bazelbuild/rules_python/issues/1643) for removing
  platform-specific content in `MODULE.bazel.lock` files.

* (wheel) The stamp variables inside the distribution name are no longer
  lower-cased when normalizing under PEP440 conventions.

### Added

* (toolchains) `python_register_toolchains` now also generates a repository
  that is suffixed with `_host`, that has a single label `:python` that is a
  symlink to the python interpreter for the host platform. The intended use is
  mainly in `repository_rule`, which are always run using `host` platform
  Python. This means that `WORKSPACE` users can now copy the `requirements.bzl`
  file for vendoring as seen in the updated `pip_parse_vendored` example.

* (runfiles) `rules_python.python.runfiles.Runfiles` now has a static `Create`
  method to make imports more ergonomic. Users should only need to import the
  `Runfiles` object to locate runfiles.

* (toolchains) `PyRuntimeInfo` now includes a `interpreter_version_info` field
  that contains the static version information for the given interpreter.
  This can be set via `py_runtime` when registering an interpreter toolchain,
  and will done automatically for the builtin interpreter versions registered via
  `python_register_toolchains`.
  Note that this only available on the Starlark implementation of the provider.

* (config_settings) Added `//python/config_settings:is_python_X.Y` config
  settings to match on minor Python version. These settings match any `X.Y`
  version instead of just an exact `X.Y.Z` version.

## [0.28.0] - 2024-01-07

[0.28.0]: https://github.com/bazelbuild/rules_python/releases/tag/0.28.0

### Changed

* **BREAKING** (pip_install) the deprecated `pip_install` macro and related
  items have been removed.

* **BREAKING** Support for Bazel 5 has been officially dropped. This release
  was only partially tested with Bazel 5 and may or may not work with Bazel 5.
  Subequent versions will no longer be tested under Bazel 5.

* (runfiles) `rules_python.python.runfiles` now directly implements type hints
  and drops support for python2 as a result.

* (toolchains) `py_runtime`, `py_runtime_pair`, and `PyRuntimeInfo` now use the
  rules_python Starlark implementation, not the one built into Bazel. NOTE: This
  only applies to Bazel 6+; Bazel 5 still uses the builtin implementation.

* (pip_parse) The parameter `experimental_requirement_cycles` may be provided a
  map of names to lists of requirements which form a dependency
  cycle. `pip_parse` will break the cycle for you transparently. This behavior
  is also available under bzlmod as
  `pip.parse(experimental_requirement_cycles={})`.

* (toolchains) `py_runtime` can now take an executable target. Note: runfiles
  from the target are not supported yet.
  ([#1612](https://github.com/bazelbuild/rules_python/issues/1612))

* (gazelle) When `python_generation_mode` is set to `file`, create one `py_binary`
  target for each file with `if __name__ == "__main__"` instead of just one
  `py_binary` for the whole module.

* (gazelle) the Gazelle manifest integrity field is now optional. If the
  `requirements` argument to `gazelle_python_manifest` is unset, no integrity
  field will be generated.

### Fixed

* (gazelle) The gazelle plugin helper was not working with Python toolchains 3.11
  and above due to a bug in the helper components not being on PYTHONPATH.

* (pip_parse) The repositories created by `whl_library` can now parse the `whl`
  METADATA and generate dependency closures irrespective of the host platform
  the generation is executed on. This can be turned on by supplying
  `experimental_target_platforms = ["all"]` to the `pip_parse` or the `bzlmod`
  equivalent. This may help in cases where fetching wheels for a different
  platform using `download_only = True` feature.
* (bzlmod pip.parse) The `pip.parse(python_interpreter)` arg now works for
  specifying a local system interpreter.
* (bzlmod pip.parse) Requirements files with duplicate entries for the same
  package (e.g. one for the package, one for an extra) now work.
* (bzlmod python.toolchain) Submodules can now (re)register the Python version
  that rules_python has set as the default.
  ([#1638](https://github.com/bazelbuild/rules_python/issues/1638))
* (whl_library) Actually use the provided patches to patch the whl_library.
  On Windows the patching may result in files with CRLF line endings, as a result
  the RECORD file consistency requirement is lifted and now a warning is emitted
  instead with a location to the patch that could be used to silence the warning.
  Copy the patch to your workspace and add it to the list if patches for the wheel
  file if you decide to do so.
* (coverage): coverage reports are now created when the version-aware
  rules are used.
  ([#1600](https://github.com/bazelbuild/rules_python/issues/1600))
* (toolchains) Workspace builds register the py cc toolchain (bzlmod already
  was). This makes e.g. `//python/cc:current_py_cc_headers` Just Work.
  ([#1669](https://github.com/bazelbuild/rules_python/issues/1669))
* (bzlmod python.toolchain) The value of `ignore_root_user_error` is now decided
  by the root module only.
  ([#1658](https://github.com/bazelbuild/rules_python/issues/1658))

### Added

* (docs) bzlmod extensions are now documented on rules-python.readthedocs.io
* (docs) Support and backwards compatibility policies have been documented.
  See https://rules-python.readthedocs.io/en/latest/support.html
* (gazelle) `file` generation mode can now also add `__init__.py` to the srcs
  attribute for every target in the package. This is enabled through a separate
  directive `python_generation_mode_per_file_include_init`.

## [0.27.0] - 2023-11-16

[0.27.0]: https://github.com/bazelbuild/rules_python/releases/tag/0.27.0

### Changed

* Make `//python/pip_install:pip_repository_bzl` `bzl_library` target internal
  as all of the publicly available symbols (etc. `package_annotation`) are
  re-exported via `//python:pip_bzl` `bzl_library`.

* (gazelle) Gazelle Python extension no longer has runtime dependencies. Using
  `GAZELLE_PYTHON_RUNTIME_DEPS` from `@rules_python_gazelle_plugin//:def.bzl` is
  no longer necessary.

* (pip_parse) The installation of `pip_parse` repository rule toolchain
  dependencies is now done as part of `py_repositories` call.

* (pip_parse) The generated `requirements.bzl` file now has an additional symbol
  `all_whl_requirements_by_package` which provides a map from the normalized
  PyPI package name to the target that provides the built wheel file. Use
  `pip_utils.normalize_name` function from `@rules_python//python:pip.bzl` to
  convert a PyPI package name to a key in the `all_whl_requirements_by_package`
  map.

* (pip_parse) The flag `incompatible_generate_aliases` has been flipped to
  `True` by default on `non-bzlmod` setups allowing users to use the same label
  strings during the transition period. For example, instead of
  `@pypi_foo//:pkg`, you can now use `@pypi//foo` or `@pypi//foo:pkg`. Other
  labels that are present in the `foo` package are `dist_info`, `whl` and
  `data`. Note, that the `@pypi_foo//:pkg` labels are still present for
  backwards compatibility.

* (gazelle) The flag `use_pip_repository_aliases` is now set to `True` by
  default, which will cause `gazelle` to change third-party dependency labels
  from `@pip_foo//:pkg` to `@pip//foo` by default.

* The `compile_pip_requirements` now defaults to `pyproject.toml` if the `src`
  or `requirements_in` attributes are unspecified, matching the upstream
  `pip-compile` behaviour more closely.

* (gazelle) Use relative paths if possible for dependencies added through
  the use of the `resolve` directive.

* (gazelle) When using `python_generation_mode file`, one `py_test` target is
  made per test file even if a target named `__test__` or a file named
  `__test__.py` exists in the same package. Previously in these cases there
  would only be one test target made.

Breaking changes:

* (pip) `pip_install` repository rule in this release has been disabled and
  will fail by default. The API symbol is going to be removed in the next
  version, please migrate to `pip_parse` as a replacement. The `pip_parse`
  rule no longer supports `requirements` attribute, please use
  `requirements_lock` instead.

* (py_wheel) switch `incompatible_normalize_name` and
  `incompatible_normalize_version` to `True` by default to enforce `PEP440`
  for wheel names built by `rules_python`.

* (tools/wheelmaker.py) drop support for Python 2 as only Python 3 is tested.

### Fixed

* Skip aliases for unloaded toolchains. Some Python versions that don't have full
  platform support, and referencing their undefined repositories can break operations
  like `bazel query rdeps(...)`.

* Python code generated from `proto_library` with `strip_import_prefix` can be imported now.

* (py_wheel) Produce deterministic wheel files and make `RECORD` file entries
  follow the order of files written to the `.whl` archive.

* (gazelle) Generate a single `py_test` target when `gazelle:python_generation_mode project`
  is used.

* (gazelle) Move waiting for the Python interpreter process to exit to the shutdown hook
  to make the usage of the `exec.Command` more idiomatic.

* (toolchains) Keep tcl subdirectory in Windows build of hermetic interpreter.

* (bzlmod) sub-modules now don't have the `//conditions:default` clause in the
  hub repos created by `pip.parse`. This should fix confusing error messages
  in case there is a misconfiguration of toolchains or a bug in `rules_python`.

### Added

* (bzlmod) Added `.whl` patching support via `patches` and `patch_strip`
  arguments to the new `pip.override` tag class.

* (pip) Support for using [PEP621](https://peps.python.org/pep-0621/) compliant
  `pyproject.toml` for creating a resolved `requirements.txt` file.

* (utils) Added a `pip_utils` struct with a `normalize_name` function to allow users
  to find out how `rules_python` would normalize a PyPI distribution name.

## [0.26.0] - 2023-10-06

### Changed

* Python version patch level bumps:
  * 3.8.15  -> 3.8.18
  * 3.9.17  -> 3.9.18
  * 3.10.12 -> 3.10.13
  * 3.11.4  -> 3.11.6

* (deps) Upgrade rules_go 0.39.1 -> 0.41.0; this is so gazelle integration works with upcoming Bazel versions

* (multi-version) The `distribs` attribute is no longer propagated. This
  attribute has been long deprecated by Bazel and shouldn't be used.

* Calling `//python:repositories.bzl#py_repositories()` is required. It has
  always been documented as necessary, but it was possible to omit it in certain
  cases. An error about `@rules_python_internal` means the `py_repositories()`
  call is missing in `WORKSPACE`.

* (bzlmod) The `pip.parse` extension will generate os/arch specific lock
  file entries on `bazel>=6.4`.


### Added

* (bzlmod, entry_point) Added {obj}`py_console_script_binary`, which
  allows adding custom dependencies to a package's entry points and customizing
  the `py_binary` rule used to build it.

* New Python versions available: `3.8.17`, `3.11.5` using
  https://github.com/indygreg/python-build-standalone/releases/tag/20230826.

* (gazelle) New `# gazelle:python_generation_mode file` directive to support
  generating one `py_library` per file.

* (python_repository) Support `netrc` and `auth_patterns` attributes to enable
  authentication against private HTTP hosts serving Python toolchain binaries.

* `//python:packaging_bzl` added, a `bzl_library` for the Starlark
  files `//python:packaging.bzl` requires.
* (py_wheel) Added the `incompatible_normalize_name` feature flag to
  normalize the package distribution name according to latest Python
  packaging standards. Defaults to `False` for the time being.
* (py_wheel) Added the `incompatible_normalize_version` feature flag
  to normalize the package version according to PEP440 standard. This
  also adds support for local version specifiers (versions with a `+`
  in them), in accordance with PEP440. Defaults to `False` for the
  time being.

* New Python versions available: `3.8.18`, `3.9.18`, `3.10.13`, `3.11.6`, `3.12.0` using
  https://github.com/indygreg/python-build-standalone/releases/tag/20231002.
  `3.12.0` support is considered beta and may have issues.

### Removed

* (bzlmod) The `entry_point` macro is no longer supported and has been removed
  in favour of the `py_console_script_binary` macro for `bzlmod` users.

* (bzlmod) The `pip.parse` no longer generates `{hub_name}_{py_version}` hub repos
  as the `entry_point` macro has been superseded by `py_console_script_binary`.

* (bzlmod) The `pip.parse` no longer generates `{hub_name}_{distribution}` hub repos.

### Fixed

* (whl_library) No longer restarts repository rule when fetching external
  dependencies improving initial build times involving external dependency
  fetching.

* (gazelle) Improve runfiles lookup hermeticity.

[0.26.0]: https://github.com/bazelbuild/rules_python/releases/tag/0.26.0

## [0.25.0] - 2023-08-22

### Changed

* Python version patch level bumps:
  * 3.9.16 -> 3.9.17
  * 3.10.9 -> 3.10.12
  * 3.11.1 -> 3.11.4
* (bzlmod) `pip.parse` can no longer automatically use the default
  Python version; this was an unreliable and unsafe behavior. The
  `python_version` arg must always be explicitly specified.

### Fixed

* (docs) Update docs to use correct bzlmod APIs and clarify how and when to use
  various APIs.
* (multi-version) The `main` arg is now correctly computed and usually optional.
* (bzlmod) `pip.parse` no longer requires a call for whatever the configured
  default Python version is.

### Added

* Created a changelog.
* (gazelle) Stop generating unnecessary imports.
* (toolchains) s390x supported for Python 3.9.17, 3.10.12, and 3.11.4.

[0.25.0]: https://github.com/bazelbuild/rules_python/releases/tag/0.25.0

## [0.24.0] - 2023-07-11

### Changed

* **BREAKING** (gazelle) Gazelle 0.30.0 or higher is required
* (bzlmod) `@python_aliases` renamed to `@python_versions
* (bzlmod) `pip.parse` arg `name` renamed to `hub_name`
* (bzlmod) `pip.parse` arg `incompatible_generate_aliases` removed and always
  true.

### Fixed

* (bzlmod) Fixing Windows Python Interpreter symlink issues
* (py_wheel) Allow twine tags and args
* (toolchain, bzlmod) Restrict coverage tool visibility under bzlmod
* (pip) Ignore temporary pyc.NNN files in wheels
* (pip) Add format() calls to glob_exclude templates
* plugin_output in py_proto_library rule

### Added

* Using Gazelle's lifecycle manager to manage external processes
* (bzlmod) `pip.parse` can be called multiple times with different Python
  versions
* (bzlmod) Allow bzlmod `pip.parse` to reference the default python toolchain and interpreter
* (bzlmod) Implementing wheel annotations via `whl_mods`
* (gazelle) support multiple requirements files in manifest generation
* (py_wheel) Support for specifying `Description-Content-Type` and `Summary` in METADATA
* (py_wheel) Support for specifying `Project-URL`
* (compile_pip_requirements) Added `generate_hashes` arg (default True) to
  control generating hashes
* (pip) Create all_data_requirements alias
* Expose Python C headers through the toolchain.

[0.24.0]: https://github.com/bazelbuild/rules_python/releases/tag/0.24.0
