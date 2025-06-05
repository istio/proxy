# Copyright 2023 The Bazel Authors. All rights reserved.
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

"""render_pkg_aliases tests"""

load("@rules_testing//lib:test_suite.bzl", "test_suite")
load(
    "//python/private/pypi:pkg_aliases.bzl",
    "get_filename_config_settings",
)  # buildifier: disable=bzl-visibility
load(
    "//python/private/pypi:render_pkg_aliases.bzl",
    "get_whl_flag_versions",
    "render_multiplatform_pkg_aliases",
    "render_pkg_aliases",
)  # buildifier: disable=bzl-visibility
load("//python/private/pypi:whl_config_setting.bzl", "whl_config_setting")  # buildifier: disable=bzl-visibility

_tests = []

def _test_empty(env):
    actual = render_pkg_aliases(
        aliases = None,
    )

    want = {}

    env.expect.that_dict(actual).contains_exactly(want)

_tests.append(_test_empty)

def _test_legacy_aliases(env):
    actual = render_pkg_aliases(
        aliases = {
            "foo": "pypi_foo",
        },
    )

    want_key = "foo/BUILD.bazel"
    want_content = """\
load("@rules_python//python/private/pypi:pkg_aliases.bzl", "pkg_aliases")

package(default_visibility = ["//visibility:public"])

pkg_aliases(
    name = "foo",
    actual = "pypi_foo",
)"""

    env.expect.that_dict(actual).contains_exactly({want_key: want_content})

_tests.append(_test_legacy_aliases)

def _test_bzlmod_aliases(env):
    # Use this function as it is used in pip_repository
    actual = render_multiplatform_pkg_aliases(
        aliases = {
            "bar-baz": {
                whl_config_setting(
                    version = "3.2",
                    config_setting = "//:my_config_setting",
                ): "pypi_32_bar_baz",
                whl_config_setting(
                    version = "3.2",
                    config_setting = "//:my_config_setting",
                    target_platforms = [
                        "cp32_linux_x86_64",
                    ],
                ): "pypi_32_bar_baz_linux_x86_64",
                whl_config_setting(
                    version = "3.2",
                    filename = "foo-0.0.0-py3-none-any.whl",
                ): "filename_repo",
                whl_config_setting(
                    version = "3.2",
                    filename = "foo-0.0.0-py3-none-any.whl",
                    target_platforms = [
                        "cp32_linux_x86_64",
                    ],
                ): "filename_repo_linux_x86_64",
            },
        },
        extra_hub_aliases = {"bar_baz": ["foo"]},
    )

    want_key = "bar_baz/BUILD.bazel"
    want_content = """\
load("@rules_python//python/private/pypi:pkg_aliases.bzl", "pkg_aliases")
load("@rules_python//python/private/pypi:whl_config_setting.bzl", "whl_config_setting")

package(default_visibility = ["//visibility:public"])

pkg_aliases(
    name = "bar_baz",
    actual = {
        "//:my_config_setting": "pypi_32_bar_baz",
        whl_config_setting(
            target_platforms = ("cp32_linux_x86_64",),
            config_setting = "//:my_config_setting",
            version = "3.2",
        ): "pypi_32_bar_baz_linux_x86_64",
        whl_config_setting(
            filename = "foo-0.0.0-py3-none-any.whl",
            version = "3.2",
        ): "filename_repo",
        whl_config_setting(
            filename = "foo-0.0.0-py3-none-any.whl",
            target_platforms = ("cp32_linux_x86_64",),
            version = "3.2",
        ): "filename_repo_linux_x86_64",
    },
    extra_aliases = ["foo"],
)"""

    env.expect.that_str(actual.pop("_config/BUILD.bazel")).equals(
        """\
load("@rules_python//python/private/pypi:config_settings.bzl", "config_settings")

config_settings(
    name = "config_settings",
    python_versions = ["3.2"],
    target_platforms = ["linux_x86_64"],
    visibility = ["//:__subpackages__"],
)""",
    )
    env.expect.that_collection(actual.keys()).contains_exactly([want_key])
    env.expect.that_str(actual[want_key]).equals(want_content)

_tests.append(_test_bzlmod_aliases)

def _test_aliases_are_created_for_all_wheels(env):
    actual = render_pkg_aliases(
        aliases = {
            "bar": {
                whl_config_setting(version = "3.1"): "pypi_31_bar",
                whl_config_setting(version = "3.2"): "pypi_32_bar",
            },
            "foo": {
                whl_config_setting(version = "3.1"): "pypi_32_foo",
                whl_config_setting(version = "3.2"): "pypi_31_foo",
            },
        },
    )

    want_files = [
        "bar/BUILD.bazel",
        "foo/BUILD.bazel",
    ]

    env.expect.that_dict(actual).keys().contains_exactly(want_files)

_tests.append(_test_aliases_are_created_for_all_wheels)

def _test_aliases_with_groups(env):
    actual = render_pkg_aliases(
        aliases = {
            "bar": {
                whl_config_setting(version = "3.1"): "pypi_31_bar",
                whl_config_setting(version = "3.2"): "pypi_32_bar",
            },
            "baz": {
                whl_config_setting(version = "3.1"): "pypi_31_baz",
                whl_config_setting(version = "3.2"): "pypi_32_baz",
            },
            "foo": {
                whl_config_setting(version = "3.1"): "pypi_32_foo",
                whl_config_setting(version = "3.2"): "pypi_31_foo",
            },
        },
        requirement_cycles = {
            "group": ["bar", "baz"],
        },
    )

    want_files = [
        "bar/BUILD.bazel",
        "foo/BUILD.bazel",
        "baz/BUILD.bazel",
        "_groups/BUILD.bazel",
    ]
    env.expect.that_dict(actual).keys().contains_exactly(want_files)

    want_key = "_groups/BUILD.bazel"

    # Just check that it contains a private whl
    env.expect.that_str(actual[want_key]).contains("//bar:_whl")

    want_key = "bar/BUILD.bazel"

    # Just check that we pass the group name
    env.expect.that_str(actual[want_key]).contains("group_name = \"group\"")

_tests.append(_test_aliases_with_groups)

def _test_empty_flag_versions(env):
    got = get_whl_flag_versions(
        settings = [],
    )
    want = {}
    env.expect.that_dict(got).contains_exactly(want)

_tests.append(_test_empty_flag_versions)

def _test_get_python_versions(env):
    got = get_whl_flag_versions(
        settings = {
            whl_config_setting(version = "3.3"): "foo",
            whl_config_setting(version = "3.2"): "foo",
        },
    )
    want = {
        "python_versions": ["3.2", "3.3"],
    }
    env.expect.that_dict(got).contains_exactly(want)

_tests.append(_test_get_python_versions)

def _test_get_python_versions_with_target_platforms(env):
    got = get_whl_flag_versions(
        settings = [
            whl_config_setting(version = "3.3", target_platforms = ["cp33_linux_x86_64"]),
            whl_config_setting(version = "3.2", target_platforms = ["cp32_linux_x86_64", "cp32_osx_aarch64"]),
        ],
    )
    want = {
        "python_versions": ["3.2", "3.3"],
        "target_platforms": [
            "linux_x86_64",
            "osx_aarch64",
        ],
    }
    env.expect.that_dict(got).contains_exactly(want)

_tests.append(_test_get_python_versions_with_target_platforms)

def _test_get_python_versions_from_filenames(env):
    got = get_whl_flag_versions(
        settings = [
            whl_config_setting(
                version = "3.3",
                filename = "foo-0.0.0-py3-none-" + plat + ".whl",
            )
            for plat in [
                "linux_x86_64",
                "manylinux_2_17_x86_64",
                "manylinux_2_14_aarch64.musllinux_1_1_aarch64",
                "musllinux_1_0_x86_64",
                "manylinux2014_x86_64.manylinux_2_17_x86_64",
                "macosx_11_0_arm64",
                "macosx_10_9_x86_64",
                "macosx_10_9_universal2",
                "windows_x86_64",
            ]
        ],
    )
    want = {
        "glibc_versions": [(2, 14), (2, 17)],
        "muslc_versions": [(1, 0), (1, 1)],
        "osx_versions": [(10, 9), (11, 0)],
        "python_versions": ["3.3"],
        "target_platforms": [
            "linux_aarch64",
            "linux_x86_64",
            "osx_aarch64",
            "osx_x86_64",
            "windows_x86_64",
        ],
    }
    env.expect.that_dict(got).contains_exactly(want)

_tests.append(_test_get_python_versions_from_filenames)

def _test_get_flag_versions_from_alias_target_platforms(env):
    got = get_whl_flag_versions(
        settings = [
            whl_config_setting(
                version = "3.3",
                filename = "foo-0.0.0-py3-none-" + plat + ".whl",
            )
            for plat in [
                "windows_x86_64",
            ]
        ] + [
            whl_config_setting(
                version = "3.3",
                filename = "foo-0.0.0-py3-none-any.whl",
                target_platforms = [
                    "cp33_linux_x86_64",
                ],
            ),
        ],
    )
    want = {
        "python_versions": ["3.3"],
        "target_platforms": [
            "linux_x86_64",
            "windows_x86_64",
        ],
    }
    env.expect.that_dict(got).contains_exactly(want)

_tests.append(_test_get_flag_versions_from_alias_target_platforms)

def _test_config_settings(
        env,
        *,
        filename,
        want,
        python_version,
        want_versions = {},
        target_platforms = [],
        glibc_versions = [],
        muslc_versions = [],
        osx_versions = []):
    got, got_default_version_settings = get_filename_config_settings(
        filename = filename,
        target_platforms = target_platforms,
        glibc_versions = glibc_versions,
        muslc_versions = muslc_versions,
        osx_versions = osx_versions,
        python_version = python_version,
    )
    env.expect.that_collection(got).contains_exactly(want)
    env.expect.that_dict(got_default_version_settings).contains_exactly(want_versions)

def _test_sdist(env):
    # Do the first test for multiple extensions
    for ext in [".tar.gz", ".zip"]:
        _test_config_settings(
            env,
            filename = "foo-0.0.1" + ext,
            python_version = "3.2",
            want = [":is_cp32_sdist"],
        )

    ext = ".zip"
    _test_config_settings(
        env,
        filename = "foo-0.0.1" + ext,
        python_version = "3.2",
        target_platforms = [
            "linux_aarch64",
            "linux_x86_64",
        ],
        want = [
            ":is_cp32_sdist_linux_aarch64",
            ":is_cp32_sdist_linux_x86_64",
        ],
    )

_tests.append(_test_sdist)

def _test_py2_py3_none_any(env):
    _test_config_settings(
        env,
        filename = "foo-0.0.1-py2.py3-none-any.whl",
        python_version = "3.2",
        want = [
            ":is_cp32_py_none_any",
        ],
    )

    _test_config_settings(
        env,
        filename = "foo-0.0.1-py2.py3-none-any.whl",
        python_version = "3.2",
        target_platforms = [
            "osx_x86_64",
        ],
        want = [":is_cp32_py_none_any_osx_x86_64"],
    )

_tests.append(_test_py2_py3_none_any)

def _test_py3_none_any(env):
    _test_config_settings(
        env,
        filename = "foo-0.0.1-py3-none-any.whl",
        python_version = "3.1",
        want = [":is_cp31_py3_none_any"],
    )

    _test_config_settings(
        env,
        filename = "foo-0.0.1-py3-none-any.whl",
        python_version = "3.1",
        target_platforms = ["linux_x86_64"],
        want = [":is_cp31_py3_none_any_linux_x86_64"],
    )

_tests.append(_test_py3_none_any)

def _test_py3_none_macosx_10_9_universal2(env):
    _test_config_settings(
        env,
        filename = "foo-0.0.1-py3-none-macosx_10_9_universal2.whl",
        python_version = "3.1",
        osx_versions = [
            (10, 9),
            (11, 0),
        ],
        want = [],
        want_versions = {
            ":is_cp31_py3_none_osx_universal2": {
                (10, 9): ":is_cp31_py3_none_osx_10_9_universal2",
                (11, 0): ":is_cp31_py3_none_osx_11_0_universal2",
            },
        },
    )

_tests.append(_test_py3_none_macosx_10_9_universal2)

def _test_cp37_abi3_linux_x86_64(env):
    _test_config_settings(
        env,
        filename = "foo-0.0.1-cp37-abi3-linux_x86_64.whl",
        python_version = "3.7",
        want = [":is_cp37_abi3_linux_x86_64"],
    )

_tests.append(_test_cp37_abi3_linux_x86_64)

def _test_cp37_abi3_windows_x86_64(env):
    _test_config_settings(
        env,
        filename = "foo-0.0.1-cp37-abi3-windows_x86_64.whl",
        python_version = "3.7",
        want = [":is_cp37_abi3_windows_x86_64"],
    )

_tests.append(_test_cp37_abi3_windows_x86_64)

def _test_cp37_abi3_manylinux_2_17_x86_64(env):
    _test_config_settings(
        env,
        filename = "foo-0.0.1-cp37-abi3-manylinux2014_x86_64.manylinux_2_17_x86_64.whl",
        python_version = "3.7",
        glibc_versions = [
            (2, 16),
            (2, 17),
            (2, 18),
        ],
        want = [],
        want_versions = {
            ":is_cp37_abi3_manylinux_x86_64": {
                (2, 17): ":is_cp37_abi3_manylinux_2_17_x86_64",
                (2, 18): ":is_cp37_abi3_manylinux_2_18_x86_64",
            },
        },
    )

_tests.append(_test_cp37_abi3_manylinux_2_17_x86_64)

def _test_cp37_abi3_manylinux_2_17_musllinux_1_1_aarch64(env):
    # I've seen such a wheel being built for `uv`
    _test_config_settings(
        env,
        filename = "foo-0.0.1-cp37-cp37-manylinux_2_17_arm64.musllinux_1_1_arm64.whl",
        python_version = "3.7",
        glibc_versions = [
            (2, 16),
            (2, 17),
            (2, 18),
        ],
        muslc_versions = [
            (1, 1),
        ],
        want = [],
        want_versions = {
            ":is_cp37_cp37_manylinux_aarch64": {
                (2, 17): ":is_cp37_cp37_manylinux_2_17_aarch64",
                (2, 18): ":is_cp37_cp37_manylinux_2_18_aarch64",
            },
            ":is_cp37_cp37_musllinux_aarch64": {
                (1, 1): ":is_cp37_cp37_musllinux_1_1_aarch64",
            },
        },
    )

_tests.append(_test_cp37_abi3_manylinux_2_17_musllinux_1_1_aarch64)

def render_pkg_aliases_test_suite(name):
    """Create the test suite.

    Args:
        name: the name of the test suite
    """
    test_suite(name = name, basic_tests = _tests)
