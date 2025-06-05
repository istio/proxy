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

""

load("@rules_testing//lib:test_suite.bzl", "test_suite")
load("//python/private:glob_excludes.bzl", "glob_excludes")  # buildifier: disable=bzl-visibility
load("//python/private/pypi:whl_library_targets.bzl", "whl_library_targets")  # buildifier: disable=bzl-visibility

_tests = []

def _test_filegroups(env):
    calls = []

    def glob(match, *, allow_empty):
        env.expect.that_bool(allow_empty).equals(True)
        return match

    whl_library_targets(
        name = "",
        dep_template = "",
        native = struct(
            filegroup = lambda **kwargs: calls.append(kwargs),
            glob = glob,
        ),
        rules = struct(),
    )

    env.expect.that_collection(calls).contains_exactly([
        {
            "name": "dist_info",
            "srcs": ["site-packages/*.dist-info/**"],
            "visibility": ["//visibility:public"],
        },
        {
            "name": "data",
            "srcs": ["data/**"],
            "visibility": ["//visibility:public"],
        },
        {
            "name": "whl",
            "srcs": [""],
            "data": [],
            "visibility": ["//visibility:public"],
        },
    ])  # buildifier: @unsorted-dict-items

_tests.append(_test_filegroups)

def _test_platforms(env):
    calls = []

    whl_library_targets(
        name = "",
        dep_template = None,
        dependencies_by_platform = {
            "@//python/config_settings:is_python_3.9": ["py39_dep"],
            "@platforms//cpu:aarch64": ["arm_dep"],
            "@platforms//os:windows": ["win_dep"],
            "cp310_linux_ppc": ["py310_linux_ppc_dep"],
            "cp39_anyos_aarch64": ["py39_arm_dep"],
            "cp39_linux_anyarch": ["py39_linux_dep"],
            "linux_x86_64": ["linux_intel_dep"],
        },
        filegroups = {},
        native = struct(
            config_setting = lambda **kwargs: calls.append(kwargs),
        ),
        rules = struct(),
    )

    env.expect.that_collection(calls).contains_exactly([
        {
            "name": "is_python_3.10_linux_ppc",
            "flag_values": {
                "@rules_python//python/config_settings:python_version_major_minor": "3.10",
            },
            "constraint_values": [
                "@platforms//cpu:ppc",
                "@platforms//os:linux",
            ],
            "visibility": ["//visibility:private"],
        },
        {
            "name": "is_python_3.9_anyos_aarch64",
            "flag_values": {
                "@rules_python//python/config_settings:python_version_major_minor": "3.9",
            },
            "constraint_values": ["@platforms//cpu:aarch64"],
            "visibility": ["//visibility:private"],
        },
        {
            "name": "is_python_3.9_linux_anyarch",
            "flag_values": {
                "@rules_python//python/config_settings:python_version_major_minor": "3.9",
            },
            "constraint_values": ["@platforms//os:linux"],
            "visibility": ["//visibility:private"],
        },
        {
            "name": "is_linux_x86_64",
            "constraint_values": [
                "@platforms//cpu:x86_64",
                "@platforms//os:linux",
            ],
            "visibility": ["//visibility:private"],
        },
    ])  # buildifier: @unsorted-dict-items

_tests.append(_test_platforms)

def _test_copy(env):
    calls = []

    whl_library_targets(
        name = "",
        dep_template = None,
        dependencies_by_platform = {},
        filegroups = {},
        copy_files = {"file_src": "file_dest"},
        copy_executables = {"exec_src": "exec_dest"},
        native = struct(),
        rules = struct(
            copy_file = lambda **kwargs: calls.append(kwargs),
        ),
    )

    env.expect.that_collection(calls).contains_exactly([
        {
            "name": "file_dest.copy",
            "out": "file_dest",
            "src": "file_src",
            "visibility": ["//visibility:public"],
        },
        {
            "is_executable": True,
            "name": "exec_dest.copy",
            "out": "exec_dest",
            "src": "exec_src",
            "visibility": ["//visibility:public"],
        },
    ])

_tests.append(_test_copy)

def _test_entrypoints(env):
    calls = []

    whl_library_targets(
        name = "",
        dep_template = None,
        dependencies_by_platform = {},
        filegroups = {},
        entry_points = {
            "fizz": "buzz.py",
        },
        native = struct(),
        rules = struct(
            py_binary = lambda **kwargs: calls.append(kwargs),
        ),
    )

    env.expect.that_collection(calls).contains_exactly([
        {
            "name": "rules_python_wheel_entry_point_fizz",
            "srcs": ["buzz.py"],
            "deps": [":pkg"],
            "imports": ["."],
            "visibility": ["//visibility:public"],
        },
    ])  # buildifier: @unsorted-dict-items

_tests.append(_test_entrypoints)

def _test_whl_and_library_deps(env):
    filegroup_calls = []
    py_library_calls = []

    whl_library_targets(
        name = "foo.whl",
        dep_template = "@pypi_{name}//:{target}",
        dependencies = ["foo", "bar-baz"],
        dependencies_by_platform = {
            "@//python/config_settings:is_python_3.9": ["py39_dep"],
            "@platforms//cpu:aarch64": ["arm_dep"],
            "@platforms//os:windows": ["win_dep"],
            "cp310_linux_ppc": ["py310_linux_ppc_dep"],
            "cp39_anyos_aarch64": ["py39_arm_dep"],
            "cp39_linux_anyarch": ["py39_linux_dep"],
            "linux_x86_64": ["linux_intel_dep"],
        },
        data_exclude = [],
        tags = ["tag1", "tag2"],
        # Overrides for testing
        filegroups = {},
        native = struct(
            filegroup = lambda **kwargs: filegroup_calls.append(kwargs),
            config_setting = lambda **_: None,
            glob = _glob,
            select = _select,
        ),
        rules = struct(
            py_library = lambda **kwargs: py_library_calls.append(kwargs),
        ),
    )

    env.expect.that_collection(filegroup_calls).contains_exactly([
        {
            "name": "whl",
            "srcs": ["foo.whl"],
            "data": [
                "@pypi_bar_baz//:whl",
                "@pypi_foo//:whl",
            ] + _select(
                {
                    Label("//python/config_settings:is_python_3.9"): ["@pypi_py39_dep//:whl"],
                    "@platforms//cpu:aarch64": ["@pypi_arm_dep//:whl"],
                    "@platforms//os:windows": ["@pypi_win_dep//:whl"],
                    ":is_python_3.10_linux_ppc": ["@pypi_py310_linux_ppc_dep//:whl"],
                    ":is_python_3.9_anyos_aarch64": ["@pypi_py39_arm_dep//:whl"],
                    ":is_python_3.9_linux_anyarch": ["@pypi_py39_linux_dep//:whl"],
                    ":is_linux_x86_64": ["@pypi_linux_intel_dep//:whl"],
                    "//conditions:default": [],
                },
            ),
            "visibility": ["//visibility:public"],
        },
    ])  # buildifier: @unsorted-dict-items
    env.expect.that_collection(py_library_calls).contains_exactly([
        {
            "name": "pkg",
            "srcs": _glob(
                ["site-packages/**/*.py"],
                exclude = [],
                allow_empty = True,
            ),
            "pyi_srcs": _glob(["site-packages/**/*.pyi"], allow_empty = True),
            "data": [] + _glob(
                ["site-packages/**/*"],
                exclude = [
                    "**/*.py",
                    "**/*.pyc",
                    "**/*.pyc.*",
                    "**/*.dist-info/RECORD",
                ] + glob_excludes.version_dependent_exclusions(),
            ),
            "imports": ["site-packages"],
            "deps": [
                "@pypi_bar_baz//:pkg",
                "@pypi_foo//:pkg",
            ] + _select(
                {
                    Label("//python/config_settings:is_python_3.9"): ["@pypi_py39_dep//:pkg"],
                    "@platforms//cpu:aarch64": ["@pypi_arm_dep//:pkg"],
                    "@platforms//os:windows": ["@pypi_win_dep//:pkg"],
                    ":is_python_3.10_linux_ppc": ["@pypi_py310_linux_ppc_dep//:pkg"],
                    ":is_python_3.9_anyos_aarch64": ["@pypi_py39_arm_dep//:pkg"],
                    ":is_python_3.9_linux_anyarch": ["@pypi_py39_linux_dep//:pkg"],
                    ":is_linux_x86_64": ["@pypi_linux_intel_dep//:pkg"],
                    "//conditions:default": [],
                },
            ),
            "tags": ["tag1", "tag2"],
            "visibility": ["//visibility:public"],
        },
    ])  # buildifier: @unsorted-dict-items

_tests.append(_test_whl_and_library_deps)

def _test_group(env):
    alias_calls = []
    py_library_calls = []

    whl_library_targets(
        name = "foo.whl",
        dep_template = "@pypi_{name}//:{target}",
        dependencies = ["foo", "bar-baz", "qux"],
        dependencies_by_platform = {
            "linux_x86_64": ["box", "box-amd64"],
            "windows_x86_64": ["fox"],
            "@platforms//os:linux": ["box"],  # buildifier: disable=unsorted-dict-items to check that we sort inside the test
        },
        tags = [],
        entry_points = {},
        data_exclude = [],
        group_name = "qux",
        group_deps = ["foo", "fox", "qux"],
        # Overrides for testing
        filegroups = {},
        native = struct(
            config_setting = lambda **_: None,
            glob = _glob,
            alias = lambda **kwargs: alias_calls.append(kwargs),
            select = _select,
        ),
        rules = struct(
            py_library = lambda **kwargs: py_library_calls.append(kwargs),
        ),
    )

    env.expect.that_collection(alias_calls).contains_exactly([
        {"name": "pkg", "actual": "@pypi__groups//:qux_pkg", "visibility": ["//visibility:public"]},
        {"name": "whl", "actual": "@pypi__groups//:qux_whl", "visibility": ["//visibility:public"]},
    ])  # buildifier: @unsorted-dict-items
    env.expect.that_collection(py_library_calls).contains_exactly([
        {
            "name": "_pkg",
            "srcs": _glob(["site-packages/**/*.py"], exclude = [], allow_empty = True),
            "pyi_srcs": _glob(["site-packages/**/*.pyi"], allow_empty = True),
            "data": [] + _glob(
                ["site-packages/**/*"],
                exclude = [
                    "**/*.py",
                    "**/*.pyc",
                    "**/*.pyc.*",
                    "**/*.dist-info/RECORD",
                ] + glob_excludes.version_dependent_exclusions(),
            ),
            "imports": ["site-packages"],
            "deps": ["@pypi_bar_baz//:pkg"] + _select({
                "@platforms//os:linux": ["@pypi_box//:pkg"],
                ":is_linux_x86_64": ["@pypi_box//:pkg", "@pypi_box_amd64//:pkg"],
                "//conditions:default": [],
            }),
            "tags": [],
            "visibility": ["@pypi__groups//:__pkg__"],
        },
    ])  # buildifier: @unsorted-dict-items

_tests.append(_test_group)

def _glob(*args, **kwargs):
    return [struct(
        glob = args,
        kwargs = kwargs,
    )]

def _select(*args, **kwargs):
    """We need to have this mock select because we still need to support bazel 6."""
    return [struct(
        select = args,
        kwargs = kwargs,
    )]

def whl_library_targets_test_suite(name):
    """create the test suite.

    args:
        name: the name of the test suite
    """
    test_suite(name = name, basic_tests = _tests)
