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
"""Common code for implementing py_library rules."""

load("@bazel_skylib//lib:dicts.bzl", "dicts")
load("@bazel_skylib//lib:paths.bzl", "paths")
load("@bazel_skylib//rules:common_settings.bzl", "BuildSettingInfo")
load(":attr_builders.bzl", "attrb")
load(
    ":attributes.bzl",
    "COMMON_ATTRS",
    "IMPORTS_ATTRS",
    "PY_SRCS_ATTRS",
    "PrecompileAttr",
    "REQUIRED_EXEC_GROUP_BUILDERS",
)
load(":builders.bzl", "builders")
load(
    ":common.bzl",
    "PYTHON_FILE_EXTENSIONS",
    "collect_cc_info",
    "collect_imports",
    "collect_runfiles",
    "create_instrumented_files_info",
    "create_library_semantics_struct",
    "create_output_group_info",
    "create_py_info",
    "filter_to_py_srcs",
    "get_imports",
    "runfiles_root_path",
)
load(":flags.bzl", "AddSrcsToRunfilesFlag", "PrecompileFlag", "VenvsSitePackages")
load(":normalize_name.bzl", "normalize_name")
load(":precompile.bzl", "maybe_precompile")
load(":py_cc_link_params_info.bzl", "PyCcLinkParamsInfo")
load(":py_info.bzl", "PyInfo", "VenvSymlinkEntry", "VenvSymlinkKind")
load(":py_internal.bzl", "py_internal")
load(":reexports.bzl", "BuiltinPyInfo")
load(":rule_builders.bzl", "ruleb")
load(
    ":toolchain_types.bzl",
    "EXEC_TOOLS_TOOLCHAIN_TYPE",
    TOOLCHAIN_TYPE = "TARGET_TOOLCHAIN_TYPE",
)
load(":version.bzl", "version")

_py_builtins = py_internal

LIBRARY_ATTRS = dicts.add(
    COMMON_ATTRS,
    PY_SRCS_ATTRS,
    IMPORTS_ATTRS,
    {
        "experimental_venvs_site_packages": lambda: attrb.Label(
            doc = """
**INTERNAL ATTRIBUTE. SHOULD ONLY BE SET BY rules_python-INTERNAL CODE.**

:::{include} /_includes/experimental_api.md
:::

A flag that decides whether the library should treat its sources as a
site-packages layout.

When the flag is `yes`, then the `srcs` files are treated as a site-packages
layout that is relative to the `imports` attribute. The `imports` attribute
can have only a single element. It is a repo-relative runfiles path.

For example, in the `my/pkg/BUILD.bazel` file, given
`srcs=["site-packages/foo/bar.py"]`, specifying
`imports=["my/pkg/site-packages"]` means `foo/bar.py` is the file path
under the binary's venv site-packages directory that should be made available (i.e.
`import foo.bar` will work).

`__init__.py` files are treated specially to provide basic support for [implicit
namespace packages](
https://packaging.python.org/en/latest/guides/packaging-namespace-packages/#native-namespace-packages).
However, the *content* of the files cannot be taken into account, merely their
presence or absence. Stated another way: [pkgutil-style namespace packages](
https://packaging.python.org/en/latest/guides/packaging-namespace-packages/#pkgutil-style-namespace-packages)
won't be understood as namespace packages; they'll be seen as regular packages. This will
likely lead to conflicts with other targets that contribute to the namespace.

:::{seealso}
This attributes populates {obj}`PyInfo.venv_symlinks`.
:::

:::{versionadded} 1.4.0
:::
:::{versionchanged} 1.5.0
The topological order has been removed and if 2 different versions of the same PyPI
package are observed, the behaviour has no guarantees except that it is deterministic
and that only one package version will be included.
:::
""",
        ),
        "_add_srcs_to_runfiles_flag": lambda: attrb.Label(
            default = "//python/config_settings:add_srcs_to_runfiles",
        ),
    },
)

def _py_library_impl_with_semantics(ctx):
    return py_library_impl(
        ctx,
        semantics = create_library_semantics_struct(
            get_imports = get_imports,
            maybe_precompile = maybe_precompile,
            get_cc_info_for_library = collect_cc_info,
        ),
    )

def py_library_impl(ctx, *, semantics):
    """Abstract implementation of py_library rule.

    Args:
        ctx: The rule ctx
        semantics: A `LibrarySemantics` struct; see `create_library_semantics_struct`

    Returns:
        A list of modern providers to propagate.
    """
    direct_sources = filter_to_py_srcs(ctx.files.srcs)

    precompile_result = semantics.maybe_precompile(ctx, direct_sources)

    required_py_files = precompile_result.keep_srcs
    required_pyc_files = []
    implicit_pyc_files = []
    implicit_pyc_source_files = direct_sources

    precompile_attr = ctx.attr.precompile
    precompile_flag = ctx.attr._precompile_flag[BuildSettingInfo].value
    if (precompile_attr == PrecompileAttr.ENABLED or
        precompile_flag == PrecompileFlag.FORCE_ENABLED):
        required_pyc_files.extend(precompile_result.pyc_files)
    else:
        implicit_pyc_files.extend(precompile_result.pyc_files)

    default_outputs = builders.DepsetBuilder()
    default_outputs.add(precompile_result.keep_srcs)
    default_outputs.add(required_pyc_files)
    default_outputs = default_outputs.build()

    runfiles = builders.RunfilesBuilder()
    if AddSrcsToRunfilesFlag.is_enabled(ctx):
        runfiles.add(required_py_files)
    runfiles.add(collect_runfiles(ctx))
    runfiles = runfiles.build(ctx)

    imports = []
    venv_symlinks = []

    imports, venv_symlinks = _get_imports_and_venv_symlinks(ctx, semantics)

    cc_info = semantics.get_cc_info_for_library(ctx)
    py_info, deps_transitive_sources, builtins_py_info = create_py_info(
        ctx,
        original_sources = direct_sources,
        required_py_files = required_py_files,
        required_pyc_files = required_pyc_files,
        implicit_pyc_files = implicit_pyc_files,
        implicit_pyc_source_files = implicit_pyc_source_files,
        imports = imports,
        venv_symlinks = venv_symlinks,
    )

    # TODO(b/253059598): Remove support for extra actions; https://github.com/bazelbuild/bazel/issues/16455
    listeners_enabled = _py_builtins.are_action_listeners_enabled(ctx)
    if listeners_enabled:
        _py_builtins.add_py_extra_pseudo_action(
            ctx = ctx,
            dependency_transitive_python_sources = deps_transitive_sources,
        )

    providers = [
        DefaultInfo(files = default_outputs, runfiles = runfiles),
        py_info,
        create_instrumented_files_info(ctx),
        PyCcLinkParamsInfo(cc_info = cc_info),
        create_output_group_info(py_info.transitive_sources, extra_groups = {}),
    ]
    if builtins_py_info:
        providers.append(builtins_py_info)
    return providers

_DEFAULT_PY_LIBRARY_DOC = """
A library of Python code that can be depended upon.

Default outputs:
* The input Python sources
* The precompiled artifacts from the sources.

NOTE: Precompilation affects which of the default outputs are included in the
resulting runfiles. See the precompile-related attributes and flags for
more information.

:::{versionchanged} 0.37.0
Source files are no longer added to the runfiles directly.
:::
"""

def _get_package_and_version(ctx):
    """Return package name and version

    If the package comes from PyPI then it will have a `.dist-info` as part of `data`, which
    allows us to get the name of the package and its version.
    """
    dist_info_metadata = None
    for d in ctx.files.data:
        # work on case insensitive FSes
        if d.basename.lower() != "metadata":
            continue

        if d.dirname.endswith(".dist-info"):
            dist_info_metadata = d

    if not dist_info_metadata:
        return None, None

    # in order to be able to have replacements in the venv, we have to add a
    # third value into the venv_symlinks, which would be the normalized
    # package name. This allows us to ensure that we can replace the `dist-info`
    # directories by checking if the package key is there.
    dist_info_dir = paths.basename(dist_info_metadata.dirname)
    package, _, _suffix = dist_info_dir.rpartition(".dist-info")
    package, _, version_str = package.rpartition("-")
    return (
        normalize_name(package),  # will have no dashes
        version.normalize(version_str),  # will have no dashes either
    )

def _get_imports_and_venv_symlinks(ctx, semantics):
    imports = depset()
    venv_symlinks = []
    if VenvsSitePackages.is_enabled(ctx):
        package, version_str = _get_package_and_version(ctx)
        venv_symlinks = _get_venv_symlinks(ctx, package, version_str)
    else:
        imports = collect_imports(ctx, semantics)
    return imports, venv_symlinks

def _get_venv_symlinks(ctx, package, version_str):
    imports = ctx.attr.imports
    if len(imports) == 0:
        fail("When venvs_site_packages is enabled, exactly one `imports` " +
             "value must be specified, got 0")
    elif len(imports) > 1:
        fail("When venvs_site_packages is enabled, exactly one `imports` " +
             "value must be specified, got {}".format(imports))
    else:
        site_packages_root = imports[0]

    if site_packages_root.endswith("/"):
        fail("The site packages root value from `imports` cannot end in " +
             "slash, got {}".format(site_packages_root))
    if site_packages_root.startswith("/"):
        fail("The site packages root value from `imports` cannot start with " +
             "slash, got {}".format(site_packages_root))

    # Append slash to prevent incorrectly prefix-string matches
    site_packages_root += "/"

    # We have to build a list of (runfiles path, site-packages path) pairs of the files to
    # create in the consuming binary's venv site-packages directory. To minimize the number of
    # files to create, we just return the paths to the directories containing the code of
    # interest.
    #
    # However, namespace packages complicate matters: multiple distributions install in the
    # same directory in site-packages. This works out because they don't overlap in their
    # files. Typically, they install to different directories within the namespace package
    # directory. We also need to ensure that we can handle a case where the main package (e.g.
    # airflow) has directories only containing data files and then namespace packages coming
    # along and being next to it.
    #
    # Lastly we have to assume python modules just being `.py` files (e.g. typing-extensions)
    # is just a single Python file.

    dir_symlinks = {}  # dirname -> runfile path
    venv_symlinks = []
    for src in ctx.files.srcs + ctx.files.data + ctx.files.pyi_srcs:
        path = _repo_relative_short_path(src.short_path)
        if not path.startswith(site_packages_root):
            continue
        path = path.removeprefix(site_packages_root)
        dir_name, _, filename = path.rpartition("/")

        if dir_name in dir_symlinks:
            # we already have this dir, this allows us to short-circuit since most of the
            # ctx.files.data might share the same directories as ctx.files.srcs
            continue

        runfiles_dir_name, _, _ = runfiles_root_path(ctx, src.short_path).partition("/")
        if dir_name:
            # This can be either:
            # * a directory with libs (e.g. numpy.libs, created by auditwheel)
            # * a directory with `__init__.py` file that potentially also needs to be
            #   symlinked.
            # * `.dist-info` directory
            #
            # This could be also regular files, that just need to be symlinked, so we will
            # add the directory here.
            dir_symlinks[dir_name] = runfiles_dir_name
        elif src.extension in PYTHON_FILE_EXTENSIONS:
            # This would be files that do not have directories and we just need to add
            # direct symlinks to them as is, we only allow Python files in here
            entry = VenvSymlinkEntry(
                kind = VenvSymlinkKind.LIB,
                link_to_path = paths.join(runfiles_dir_name, site_packages_root, filename),
                package = package,
                version = version_str,
                venv_path = filename,
            )
            venv_symlinks.append(entry)

    # Sort so that we encounter `foo` before `foo/bar`. This ensures we
    # see the top-most explicit package first.
    dirnames = sorted(dir_symlinks.keys())
    first_level_explicit_packages = []
    for d in dirnames:
        is_sub_package = False
        for existing in first_level_explicit_packages:
            # Suffix with / to prevent foo matching foobar
            if d.startswith(existing + "/"):
                is_sub_package = True
                break
        if not is_sub_package:
            first_level_explicit_packages.append(d)

    for dirname in first_level_explicit_packages:
        prefix = dir_symlinks[dirname]
        entry = VenvSymlinkEntry(
            kind = VenvSymlinkKind.LIB,
            link_to_path = paths.join(prefix, site_packages_root, dirname),
            package = package,
            version = version_str,
            venv_path = dirname,
        )
        venv_symlinks.append(entry)

    return venv_symlinks

def _repo_relative_short_path(short_path):
    # Convert `../+pypi+foo/some/file.py` to `some/file.py`
    if short_path.startswith("../"):
        return short_path[3:].partition("/")[2]
    else:
        return short_path

_MaybeBuiltinPyInfo = [BuiltinPyInfo] if BuiltinPyInfo != None else []

# NOTE: Exported publicaly
def create_py_library_rule_builder():
    """Create a rule builder for a py_library.

    :::{include} /_includes/volatile_api.md
    :::

    :::{versionadded} 1.3.0
    :::

    Returns:
        {type}`ruleb.Rule` with the necessary settings
        for creating a `py_library` rule.
    """
    builder = ruleb.Rule(
        implementation = _py_library_impl_with_semantics,
        doc = _DEFAULT_PY_LIBRARY_DOC,
        exec_groups = dict(REQUIRED_EXEC_GROUP_BUILDERS),
        attrs = LIBRARY_ATTRS,
        fragments = ["py"],
        provides = [PyCcLinkParamsInfo, PyInfo] + _MaybeBuiltinPyInfo,
        toolchains = [
            ruleb.ToolchainType(TOOLCHAIN_TYPE, mandatory = False),
            ruleb.ToolchainType(EXEC_TOOLS_TOOLCHAIN_TYPE, mandatory = False),
        ],
    )
    return builder
