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
load(
    "//python/private/pypi:whl_library_targets.bzl",
    "whl_library_targets",
    "whl_library_targets_from_requires",
)  # buildifier: disable=bzl-visibility

_tests = []

def _test_filegroups(env):
    calls = []

    def glob(include, *, exclude = [], allow_empty):
        _ = exclude  # @unused
        env.expect.that_bool(allow_empty).equals(True)
        return include

    whl_library_targets(
        name = "",
        dep_template = "",
        native = struct(
            filegroup = lambda **kwargs: calls.append(kwargs),
            glob = glob,
        ),
        rules = struct(),
    )

    env.expect.that_collection(calls, expr = "filegroup calls").contains_exactly([
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
            "name": "extracted_whl_files",
            "srcs": ["**"],
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
            "cp310.11_linux_ppc64le": ["full_version_dep"],
            "cp310_linux_ppc64le": ["py310_linux_ppc64le_dep"],
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
            "name": "is_python_3.10.11_linux_ppc64le",
            "visibility": ["//visibility:private"],
            "constraint_values": [
                "@platforms//cpu:ppc64le",
                "@platforms//os:linux",
            ],
            "flag_values": {
                Label("//python/config_settings:python_version"): "3.10.11",
            },
        },
        {
            "name": "is_python_3.10_linux_ppc64le",
            "visibility": ["//visibility:private"],
            "constraint_values": [
                "@platforms//cpu:ppc64le",
                "@platforms//os:linux",
            ],
            "flag_values": {
                Label("//python/config_settings:python_version"): "3.10",
            },
        },
        {
            "name": "is_linux_x86_64",
            "visibility": ["//visibility:private"],
            "constraint_values": [
                "@platforms//cpu:x86_64",
                "@platforms//os:linux",
            ],
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

def _test_whl_and_library_deps_from_requires(env):
    filegroup_calls = []
    py_library_calls = []
    env_marker_setting_calls = []

    mock_glob = _mock_glob()

    mock_glob.results.append(["site-packages/foo/SRCS.py"])
    mock_glob.results.append(["site-packages/foo/DATA.txt"])
    mock_glob.results.append(["site-packages/foo/PYI.pyi"])

    whl_library_targets_from_requires(
        name = "foo-0-py3-none-any.whl",
        metadata_name = "Foo",
        metadata_version = "0",
        dep_template = "@pypi//{name}:{target}",
        requires_dist = [
            "foo",  # this self-edge will be ignored
            "bar",
            "bar-baz; python_version < \"8.2\"",
            "booo",  # this is effectively excluded due to the list below
        ],
        include = ["foo", "bar", "bar_baz"],
        data_exclude = [],
        # Overrides for testing
        filegroups = {},
        native = struct(
            filegroup = lambda **kwargs: filegroup_calls.append(kwargs),
            config_setting = lambda **_: None,
            glob = mock_glob.glob,
        ),
        rules = struct(
            py_library = lambda **kwargs: py_library_calls.append(kwargs),
            env_marker_setting = lambda **kwargs: env_marker_setting_calls.append(kwargs),
            create_inits = lambda *args, **kwargs: ["_create_inits_target"],
        ),
    )

    env.expect.that_collection(filegroup_calls).contains_exactly([
        {
            "name": "whl",
            "srcs": ["foo-0-py3-none-any.whl"],
            "data": ["@pypi//bar:whl"] + select({
                ":is_include_bar_baz_true": ["@pypi//bar_baz:whl"],
                "//conditions:default": [],
            }),
            "visibility": ["//visibility:public"],
        },
    ])  # buildifier: @unsorted-dict-items

    env.expect.that_collection(py_library_calls).has_size(1)
    if len(py_library_calls) != 1:
        return
    py_library_call = py_library_calls[0]

    env.expect.that_dict(py_library_call).contains_exactly({
        "name": "pkg",
        "srcs": ["site-packages/foo/SRCS.py"] + select({
            Label("//python/config_settings:is_venvs_site_packages"): [],
            "//conditions:default": ["_create_inits_target"],
        }),
        "pyi_srcs": ["site-packages/foo/PYI.pyi"],
        "data": ["site-packages/foo/DATA.txt"],
        "imports": ["site-packages"],
        "deps": ["@pypi//bar:pkg"] + select({
            ":is_include_bar_baz_true": ["@pypi//bar_baz:pkg"],
            "//conditions:default": [],
        }),
        "tags": ["pypi_name=Foo", "pypi_version=0"],
        "visibility": ["//visibility:public"],
        "experimental_venvs_site_packages": Label("//python/config_settings:venvs_site_packages"),
        "namespace_package_files": [] + select({
            Label("//python/config_settings:is_venvs_site_packages"): [],
            "//conditions:default": ["_create_inits_target"],
        }),
    })  # buildifier: @unsorted-dict-items

    env.expect.that_collection(mock_glob.calls).contains_exactly([
        # srcs call
        _glob_call(
            ["site-packages/**/*.py"],
            exclude = [],
            allow_empty = True,
        ),
        # data call
        _glob_call(
            ["site-packages/**/*"],
            exclude = [
                "**/*.py",
                "**/*.pyc",
                "**/*.pyc.*",
                "**/*.dist-info/RECORD",
            ],
            allow_empty = True,
        ),
        # pyi call
        _glob_call(["site-packages/**/*.pyi"], allow_empty = True),
    ])

    env.expect.that_collection(env_marker_setting_calls).contains_exactly([
        {
            "name": "include_bar_baz",
            "expression": "python_version < \"8.2\"",
            "visibility": ["//visibility:private"],
        },
    ])  # buildifier: @unsorted-dict-items

_tests.append(_test_whl_and_library_deps_from_requires)

def _test_whl_and_library_deps(env):
    filegroup_calls = []
    py_library_calls = []
    mock_glob = _mock_glob()
    mock_glob.results.append(["site-packages/foo/SRCS.py"])
    mock_glob.results.append(["site-packages/foo/DATA.txt"])
    mock_glob.results.append(["site-packages/foo/PYI.pyi"])

    whl_library_targets(
        name = "foo.whl",
        dep_template = "@pypi_{name}//:{target}",
        dependencies = ["foo", "bar-baz"],
        dependencies_by_platform = {
            "@//python/config_settings:is_python_3.9": ["py39_dep"],
            "@platforms//cpu:aarch64": ["arm_dep"],
            "@platforms//os:windows": ["win_dep"],
            "cp310_linux_ppc64le": ["py310_linux_ppc64le_dep"],
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
            glob = mock_glob.glob,
        ),
        rules = struct(
            py_library = lambda **kwargs: py_library_calls.append(kwargs),
            create_inits = lambda **kwargs: ["_create_inits_target"],
        ),
    )

    env.expect.that_collection(filegroup_calls).contains_exactly([
        {
            "name": "whl",
            "srcs": ["foo.whl"],
            "data": [
                "@pypi_bar_baz//:whl",
                "@pypi_foo//:whl",
            ] + select(
                {
                    Label("//python/config_settings:is_python_3.9"): ["@pypi_py39_dep//:whl"],
                    "@platforms//cpu:aarch64": ["@pypi_arm_dep//:whl"],
                    "@platforms//os:windows": ["@pypi_win_dep//:whl"],
                    ":is_python_3.10_linux_ppc64le": ["@pypi_py310_linux_ppc64le_dep//:whl"],
                    ":is_python_3.9_anyos_aarch64": ["@pypi_py39_arm_dep//:whl"],
                    ":is_python_3.9_linux_anyarch": ["@pypi_py39_linux_dep//:whl"],
                    ":is_linux_x86_64": ["@pypi_linux_intel_dep//:whl"],
                    "//conditions:default": [],
                },
            ),
            "visibility": ["//visibility:public"],
        },
    ])  # buildifier: @unsorted-dict-items

    env.expect.that_collection(py_library_calls).has_size(1)
    if len(py_library_calls) != 1:
        return
    env.expect.that_dict(py_library_calls[0]).contains_exactly({
        "name": "pkg",
        "srcs": ["site-packages/foo/SRCS.py"] + select({
            Label("//python/config_settings:is_venvs_site_packages"): [],
            "//conditions:default": ["_create_inits_target"],
        }),
        "pyi_srcs": ["site-packages/foo/PYI.pyi"],
        "data": ["site-packages/foo/DATA.txt"],
        "imports": ["site-packages"],
        "deps": [
            "@pypi_bar_baz//:pkg",
            "@pypi_foo//:pkg",
        ] + select(
            {
                Label("//python/config_settings:is_python_3.9"): ["@pypi_py39_dep//:pkg"],
                "@platforms//cpu:aarch64": ["@pypi_arm_dep//:pkg"],
                "@platforms//os:windows": ["@pypi_win_dep//:pkg"],
                ":is_python_3.10_linux_ppc64le": ["@pypi_py310_linux_ppc64le_dep//:pkg"],
                ":is_python_3.9_anyos_aarch64": ["@pypi_py39_arm_dep//:pkg"],
                ":is_python_3.9_linux_anyarch": ["@pypi_py39_linux_dep//:pkg"],
                ":is_linux_x86_64": ["@pypi_linux_intel_dep//:pkg"],
                "//conditions:default": [],
            },
        ),
        "tags": ["tag1", "tag2"],
        "visibility": ["//visibility:public"],
        "experimental_venvs_site_packages": Label("//python/config_settings:venvs_site_packages"),
        "namespace_package_files": [] + select({
            Label("//python/config_settings:is_venvs_site_packages"): [],
            "//conditions:default": ["_create_inits_target"],
        }),
    })  # buildifier: @unsorted-dict-items

_tests.append(_test_whl_and_library_deps)

def _test_group(env):
    alias_calls = []
    py_library_calls = []

    mock_glob = _mock_glob()
    mock_glob.results.append(["site-packages/foo/srcs.py"])
    mock_glob.results.append(["site-packages/foo/data.txt"])
    mock_glob.results.append(["site-packages/foo/pyi.pyi"])

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
            glob = mock_glob.glob,
            alias = lambda **kwargs: alias_calls.append(kwargs),
        ),
        rules = struct(
            py_library = lambda **kwargs: py_library_calls.append(kwargs),
            create_inits = lambda **kwargs: ["_create_inits_target"],
        ),
    )

    env.expect.that_collection(alias_calls).contains_exactly([
        {"name": "pkg", "actual": "@pypi__config//_groups:qux_pkg", "visibility": ["//visibility:public"]},
        {"name": "whl", "actual": "@pypi__config//_groups:qux_whl", "visibility": ["//visibility:public"]},
    ])  # buildifier: @unsorted-dict-items

    env.expect.that_collection(py_library_calls).has_size(1)
    if len(py_library_calls) != 1:
        return

    py_library_call = py_library_calls[0]
    env.expect.where(case = "verify py library call").that_dict(
        py_library_call,
    ).contains_exactly({
        "name": "_pkg",
        "srcs": ["site-packages/foo/srcs.py"] + select({
            Label("//python/config_settings:is_venvs_site_packages"): [],
            "//conditions:default": ["_create_inits_target"],
        }),
        "pyi_srcs": ["site-packages/foo/pyi.pyi"],
        "data": ["site-packages/foo/data.txt"],
        "imports": ["site-packages"],
        "deps": ["@pypi_bar_baz//:pkg"] + select({
            "@platforms//os:linux": ["@pypi_box//:pkg"],
            ":is_linux_x86_64": ["@pypi_box//:pkg", "@pypi_box_amd64//:pkg"],
            "//conditions:default": [],
        }),
        "tags": [],
        "visibility": ["@pypi__config//_groups:__pkg__"],
        "experimental_venvs_site_packages": Label("//python/config_settings:venvs_site_packages"),
        "namespace_package_files": [] + select({
            Label("//python/config_settings:is_venvs_site_packages"): [],
            "//conditions:default": ["_create_inits_target"],
        }),
    })  # buildifier: @unsorted-dict-items

    env.expect.that_collection(mock_glob.calls, expr = "glob calls").contains_exactly([
        _glob_call(["site-packages/**/*.py"], exclude = [], allow_empty = True),
        _glob_call(["site-packages/**/*"], exclude = [
            "**/*.py",
            "**/*.pyc",
            "**/*.pyc.*",
            "**/*.dist-info/RECORD",
        ], allow_empty = True),
        _glob_call(["site-packages/**/*.pyi"], allow_empty = True),
    ])

_tests.append(_test_group)

def _glob_call(*args, **kwargs):
    return struct(
        glob = args,
        kwargs = kwargs,
    )

def _mock_glob():
    # buildifier: disable=uninitialized
    def glob(*args, **kwargs):
        mock.calls.append(_glob_call(*args, **kwargs))
        if not mock.results:
            fail("Mock glob missing for invocation: args={} kwargs={}".format(
                args,
                kwargs,
            ))
        return mock.results.pop(0)

    mock = struct(
        calls = [],
        results = [],
        glob = glob,
    )
    return mock

def whl_library_targets_test_suite(name):
    """create the test suite.

    args:
        name: the name of the test suite
    """
    test_suite(name = name, basic_tests = _tests)
