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
    "collect_cc_info",
    "collect_imports",
    "collect_runfiles",
    "create_instrumented_files_info",
    "create_output_group_info",
    "create_py_info",
    "filter_to_py_srcs",
)
load(":common_labels.bzl", "labels")
load(":flags.bzl", "AddSrcsToRunfilesFlag", "PrecompileFlag", "VenvsSitePackages")
load(":normalize_name.bzl", "normalize_name")
load(":precompile.bzl", "maybe_precompile")
load(":py_cc_link_params_info.bzl", "PyCcLinkParamsInfo")
load(":py_info.bzl", "PyInfo")
load(":reexports.bzl", "BuiltinPyInfo")
load(":rule_builders.bzl", "ruleb")
load(
    ":toolchain_types.bzl",
    "EXEC_TOOLS_TOOLCHAIN_TYPE",
    TOOLCHAIN_TYPE = "TARGET_TOOLCHAIN_TYPE",
)
load(":venv_runfiles.bzl", "get_venv_symlinks")
load(":version.bzl", "version")

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
        "namespace_package_files": lambda: attrb.LabelList(
            allow_empty = True,
            allow_files = True,
            doc = """
Files whose directories are namespace packages.

When {obj}`--venvs_site_packages=yes` is set, this helps inform which directories should be
treated as namespace packages and expect files from other targets to be contributed.
This allows optimizing the generation of symlinks to be cheaper at analysis time.
:::{versionadded} 1.8.0
:::
""",
        ),
        "_add_srcs_to_runfiles_flag": lambda: attrb.Label(
            default = labels.ADD_SRCS_TO_RUNFILES,
        ),
    },
)

def py_library_impl(ctx):
    """Abstract implementation of py_library rule.

    Args:
        ctx: The rule ctx

    Returns:
        A list of modern providers to propagate.
    """
    direct_sources = filter_to_py_srcs(ctx.files.srcs)

    precompile_result = maybe_precompile(ctx, direct_sources)

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

    imports, venv_symlinks = _get_imports_and_venv_symlinks(ctx)

    cc_info = collect_cc_info(ctx)
    py_info, builtins_py_info = create_py_info(
        ctx,
        original_sources = direct_sources,
        required_py_files = required_py_files,
        required_pyc_files = required_pyc_files,
        implicit_pyc_files = implicit_pyc_files,
        implicit_pyc_source_files = implicit_pyc_source_files,
        imports = imports,
        venv_symlinks = venv_symlinks,
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

def _get_imports_and_venv_symlinks(ctx):
    imports = depset()
    venv_symlinks = []
    if VenvsSitePackages.is_enabled(ctx):
        package, version_str = _get_package_and_version(ctx)

        # NOTE: Already a list, but buildifier thinks its a depset and
        # adds to_list() calls later.
        imports = list(ctx.attr.imports)
        if len(imports) == 0:
            fail("When venvs_site_packages is enabled, exactly one `imports` " +
                 "value must be specified, got 0")
        elif len(imports) > 1:
            fail("When venvs_site_packages is enabled, exactly one `imports` " +
                 "value must be specified, got {}".format(imports))

        site_packages_root = paths.normalize(paths.join(
            ctx.label.package,
            imports[0],
        ))

        # Prevent escaping out of the repo root.
        if site_packages_root.startswith("../") or site_packages_root == "..":
            fail(("Invalid `imports` value '{}': resolves to '{}' which is " +
                  "above the repo root").format(
                imports[0],
                site_packages_root,
            ))
        venv_symlinks = get_venv_symlinks(
            ctx,
            ctx.files.srcs + ctx.files.data + ctx.files.pyi_srcs,
            package,
            version_str,
            site_packages_root = site_packages_root,
            namespace_package_files = ctx.files.namespace_package_files,
        )
    else:
        imports = collect_imports(ctx)
    return imports, venv_symlinks

_MaybeBuiltinPyInfo = [BuiltinPyInfo] if BuiltinPyInfo != None else []

# NOTE: Exported publicaly
def create_py_library_rule_builder():
    """Create a rule builder for a py_library.

    :::{include} /_includes/volatile_api.md
    :::

    :::{versionadded} 1.3.0
    :::

    Returns:
        {obj}`ruleb.Rule` with the necessary settings
        for creating a `py_library` rule.
    """
    builder = ruleb.Rule(
        implementation = py_library_impl,
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
