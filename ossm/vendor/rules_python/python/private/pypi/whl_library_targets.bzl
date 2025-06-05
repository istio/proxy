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

"""Macro to generate all of the targets present in a {obj}`whl_library`."""

load("@bazel_skylib//rules:copy_file.bzl", "copy_file")
load("//python:py_binary.bzl", "py_binary")
load("//python:py_library.bzl", "py_library")
load("//python/private:glob_excludes.bzl", "glob_excludes")
load("//python/private:normalize_name.bzl", "normalize_name")
load(
    ":labels.bzl",
    "DATA_LABEL",
    "DIST_INFO_LABEL",
    "PY_LIBRARY_IMPL_LABEL",
    "PY_LIBRARY_PUBLIC_LABEL",
    "WHEEL_ENTRY_POINT_PREFIX",
    "WHEEL_FILE_IMPL_LABEL",
    "WHEEL_FILE_PUBLIC_LABEL",
)

def whl_library_targets(
        *,
        name,
        dep_template,
        data_exclude = [],
        srcs_exclude = [],
        tags = [],
        filegroups = {
            DIST_INFO_LABEL: ["site-packages/*.dist-info/**"],
            DATA_LABEL: ["data/**"],
        },
        dependencies = [],
        dependencies_by_platform = {},
        group_deps = [],
        group_name = "",
        data = [],
        copy_files = {},
        copy_executables = {},
        entry_points = {},
        native = native,
        rules = struct(
            copy_file = copy_file,
            py_binary = py_binary,
            py_library = py_library,
        )):
    """Create all of the whl_library targets.

    Args:
        name: {type}`str` The file to match for including it into the `whl`
            filegroup. This may be also parsed to generate extra metadata.
        dep_template: {type}`str` The dep_template to use for dependency
            interpolation.
        tags: {type}`list[str]` The tags set on the `py_library`.
        dependencies: {type}`list[str]` A list of dependencies.
        dependencies_by_platform: {type}`dict[str, list[str]]` A list of
            dependencies by platform key.
        filegroups: {type}`dict[str, list[str]]` A dictionary of the target
            names and the glob matches.
        group_name: {type}`str` name of the dependency group (if any) which
            contains this library. If set, this library will behave as a shim
            to group implementation rules which will provide simultaneously
            installed dependencies which would otherwise form a cycle.
        group_deps: {type}`list[str]` names of fellow members of the group (if
            any). These will be excluded from generated deps lists so as to avoid
            direct cycles. These dependencies will be provided at runtime by the
            group rules which wrap this library and its fellows together.
        copy_executables: {type}`dict[str, str]` The mapping between src and
            dest locations for the targets.
        copy_files: {type}`dict[str, str]` The mapping between src and
            dest locations for the targets.
        data_exclude: {type}`list[str]` The globs for data attribute exclusion
            in `py_library`.
        srcs_exclude: {type}`list[str]` The globs for srcs attribute exclusion
            in `py_library`.
        data: {type}`list[str]` A list of labels to include as part of the `data` attribute in `py_library`.
        entry_points: {type}`dict[str, str]` The mapping between the script
            name and the python file to use. DEPRECATED.
        native: {type}`native` The native struct for overriding in tests.
        rules: {type}`struct` A struct with references to rules for creating targets.
    """
    _ = name  # buildifier: @unused

    dependencies = sorted([normalize_name(d) for d in dependencies])
    dependencies_by_platform = {
        platform: sorted([normalize_name(d) for d in deps])
        for platform, deps in dependencies_by_platform.items()
    }
    tags = sorted(tags)
    data = [] + data

    for filegroup_name, glob in filegroups.items():
        native.filegroup(
            name = filegroup_name,
            srcs = native.glob(glob, allow_empty = True),
            visibility = ["//visibility:public"],
        )

    for src, dest in copy_files.items():
        rules.copy_file(
            name = dest + ".copy",
            src = src,
            out = dest,
            visibility = ["//visibility:public"],
        )
        data.append(dest)
    for src, dest in copy_executables.items():
        rules.copy_file(
            name = dest + ".copy",
            src = src,
            out = dest,
            is_executable = True,
            visibility = ["//visibility:public"],
        )
        data.append(dest)

    _config_settings(
        dependencies_by_platform.keys(),
        native = native,
        visibility = ["//visibility:private"],
    )

    # TODO @aignas 2024-10-25: remove the entry_point generation once
    # `py_console_script_binary` is the only way to use entry points.
    for entry_point, entry_point_script_name in entry_points.items():
        rules.py_binary(
            name = "{}_{}".format(WHEEL_ENTRY_POINT_PREFIX, entry_point),
            # Ensure that this works on Windows as well - script may have Windows path separators.
            srcs = [entry_point_script_name.replace("\\", "/")],
            # This makes this directory a top-level in the python import
            # search path for anything that depends on this.
            imports = ["."],
            deps = [":" + PY_LIBRARY_PUBLIC_LABEL],
            visibility = ["//visibility:public"],
        )

    # Ensure this list is normalized
    # Note: mapping used as set
    group_deps = {
        normalize_name(d): True
        for d in group_deps
    }

    dependencies = [
        d
        for d in dependencies
        if d not in group_deps
    ]
    dependencies_by_platform = {
        p: deps
        for p, deps in dependencies_by_platform.items()
        for deps in [[d for d in deps if d not in group_deps]]
        if deps
    }

    # If this library is a member of a group, its public label aliases need to
    # point to the group implementation rule not the implementation rules. We
    # also need to mark the implementation rules as visible to the group
    # implementation.
    if group_name and "//:" in dep_template:
        # This is the legacy behaviour where the group library is outside the hub repo
        label_tmpl = dep_template.format(
            name = "_groups",
            target = normalize_name(group_name) + "_{}",
        )
        impl_vis = [dep_template.format(
            name = "_groups",
            target = "__pkg__",
        )]

        native.alias(
            name = PY_LIBRARY_PUBLIC_LABEL,
            actual = label_tmpl.format(PY_LIBRARY_PUBLIC_LABEL),
            visibility = ["//visibility:public"],
        )
        native.alias(
            name = WHEEL_FILE_PUBLIC_LABEL,
            actual = label_tmpl.format(WHEEL_FILE_PUBLIC_LABEL),
            visibility = ["//visibility:public"],
        )
        py_library_label = PY_LIBRARY_IMPL_LABEL
        whl_file_label = WHEEL_FILE_IMPL_LABEL

    elif group_name:
        py_library_label = PY_LIBRARY_PUBLIC_LABEL
        whl_file_label = WHEEL_FILE_PUBLIC_LABEL
        impl_vis = [dep_template.format(name = "", target = "__subpackages__")]

    else:
        py_library_label = PY_LIBRARY_PUBLIC_LABEL
        whl_file_label = WHEEL_FILE_PUBLIC_LABEL
        impl_vis = ["//visibility:public"]

    if hasattr(native, "filegroup"):
        native.filegroup(
            name = whl_file_label,
            srcs = [name],
            data = _deps(
                deps = dependencies,
                deps_by_platform = dependencies_by_platform,
                tmpl = dep_template.format(name = "{}", target = WHEEL_FILE_PUBLIC_LABEL),
                # NOTE @aignas 2024-10-28: Actually, `select` is not part of
                # `native`, but in order to support bazel 6.4 in unit tests, I
                # have to somehow pass the `select` implementation in the unit
                # tests and I chose this to be routed through the `native`
                # struct. So, tests` will be successful in `getattr` and the
                # real code will use the fallback provided here.
                select = getattr(native, "select", select),
            ),
            visibility = impl_vis,
        )

    if hasattr(rules, "py_library"):
        # NOTE: pyi files should probably be excluded because they're carried
        # by the pyi_srcs attribute. However, historical behavior included
        # them in data and some tools currently rely on that.
        _data_exclude = [
            "**/*.py",
            "**/*.pyc",
            "**/*.pyc.*",  # During pyc creation, temp files named *.pyc.NNNN are created
            # RECORD is known to contain sha256 checksums of files which might include the checksums
            # of generated files produced when wheels are installed. The file is ignored to avoid
            # Bazel caching issues.
            "**/*.dist-info/RECORD",
        ] + glob_excludes.version_dependent_exclusions()
        for item in data_exclude:
            if item not in _data_exclude:
                _data_exclude.append(item)

        rules.py_library(
            name = py_library_label,
            srcs = native.glob(
                ["site-packages/**/*.py"],
                exclude = srcs_exclude,
                # Empty sources are allowed to support wheels that don't have any
                # pure-Python code, e.g. pymssql, which is written in Cython.
                allow_empty = True,
            ),
            pyi_srcs = native.glob(
                ["site-packages/**/*.pyi"],
                allow_empty = True,
            ),
            data = data + native.glob(
                ["site-packages/**/*"],
                exclude = _data_exclude,
            ),
            # This makes this directory a top-level in the python import
            # search path for anything that depends on this.
            imports = ["site-packages"],
            deps = _deps(
                deps = dependencies,
                deps_by_platform = dependencies_by_platform,
                tmpl = dep_template.format(name = "{}", target = PY_LIBRARY_PUBLIC_LABEL),
                select = getattr(native, "select", select),
            ),
            tags = tags,
            visibility = impl_vis,
        )

def _config_settings(dependencies_by_platform, native = native, **kwargs):
    """Generate config settings for the targets.

    Args:
        dependencies_by_platform: {type}`list[str]` platform keys, can be
            one of the following formats:
            * `//conditions:default`
            * `@platforms//os:{value}`
            * `@platforms//cpu:{value}`
            * `@//python/config_settings:is_python_3.{minor_version}`
            * `{os}_{cpu}`
            * `cp3{minor_version}_{os}_{cpu}`
        native: {type}`native` The native struct for overriding in tests.
        **kwargs: Extra kwargs to pass to the rule.
    """
    for p in dependencies_by_platform:
        if p.startswith("@") or p.endswith("default"):
            continue

        abi, _, tail = p.partition("_")
        if not abi.startswith("cp"):
            tail = p
            abi = ""

        os, _, arch = tail.partition("_")
        os = "" if os == "anyos" else os
        arch = "" if arch == "anyarch" else arch

        _kwargs = dict(kwargs)
        if arch:
            _kwargs.setdefault("constraint_values", []).append("@platforms//cpu:{}".format(arch))
        if os:
            _kwargs.setdefault("constraint_values", []).append("@platforms//os:{}".format(os))

        if abi:
            _kwargs["flag_values"] = {
                "@rules_python//python/config_settings:python_version_major_minor": "3.{minor_version}".format(
                    minor_version = abi[len("cp3"):],
                ),
            }

        native.config_setting(
            name = "is_{name}".format(
                name = p.replace("cp3", "python_3."),
            ),
            **_kwargs
        )

def _plat_label(plat):
    if plat.endswith("default"):
        return plat
    elif plat.startswith("@//"):
        return Label(plat.strip("@"))
    elif plat.startswith("@"):
        return plat
    else:
        return ":is_" + plat.replace("cp3", "python_3.")

def _deps(deps, deps_by_platform, tmpl, select = select):
    deps = [tmpl.format(d) for d in sorted(deps)]

    if not deps_by_platform:
        return deps

    deps_by_platform = {
        _plat_label(p): [
            tmpl.format(d)
            for d in sorted(deps)
        ]
        for p, deps in sorted(deps_by_platform.items())
    }

    # Add the default, which means that we will be just using the dependencies in
    # `deps` for platforms that are not handled in a special way by the packages
    deps_by_platform.setdefault("//conditions:default", [])

    if not deps:
        return select(deps_by_platform)
    else:
        return deps + select(deps_by_platform)
