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

"""pkg_aliases tests"""

load("@rules_testing//lib:test_suite.bzl", "test_suite")
load("//python/private/pypi:config_settings.bzl", "config_settings")  # buildifier: disable=bzl-visibility
load(
    "//python/private/pypi:pkg_aliases.bzl",
    "multiplatform_whl_aliases",
    "pkg_aliases",
)  # buildifier: disable=bzl-visibility
load("//python/private/pypi:whl_config_setting.bzl", "whl_config_setting")  # buildifier: disable=bzl-visibility

_tests = []

def _test_legacy_aliases(env):
    got = {}
    pkg_aliases(
        name = "foo",
        actual = "repo",
        native = struct(
            alias = lambda name, actual: got.update({name: actual}),
        ),
        extra_aliases = ["my_special"],
    )

    # buildifier: disable=unsorted-dict-items
    want = {
        "foo": ":pkg",
        "pkg": "@repo//:pkg",
        "whl": "@repo//:whl",
        "data": "@repo//:data",
        "dist_info": "@repo//:dist_info",
        "my_special": "@repo//:my_special",
    }

    env.expect.that_dict(got).contains_exactly(want)

_tests.append(_test_legacy_aliases)

def _test_config_setting_aliases(env):
    # Use this function as it is used in pip_repository
    got = {}
    actual_no_match_error = []

    def mock_select(value, no_match_error = None):
        if no_match_error and no_match_error not in actual_no_match_error:
            actual_no_match_error.append(no_match_error)
        return value

    pkg_aliases(
        name = "bar_baz",
        actual = {
            "//:my_config_setting": "bar_baz_repo",
        },
        extra_aliases = ["my_special"],
        native = struct(
            alias = lambda *, name, actual, visibility = None, tags = None: got.update({name: actual}),
        ),
        select = mock_select,
    )

    # buildifier: disable=unsorted-dict-items
    want = {
        "pkg": {
            "//:my_config_setting": "@bar_baz_repo//:pkg",
            "//conditions:default": "_no_matching_repository",
        },
        # This will be printing the current config values and will make sure we
        # have an error.
        "_no_matching_repository": {Label("//python/config_settings:is_not_matching_current_config"): Label("//python:none")},
    }
    env.expect.that_dict(got).contains_at_least(want)
    env.expect.that_collection(actual_no_match_error).has_size(1)
    env.expect.that_str(actual_no_match_error[0]).contains("""\
configuration settings:
    //:my_config_setting

""")
    env.expect.that_str(actual_no_match_error[0]).contains(
        "//python/config_settings:current_config=fail",
    )

_tests.append(_test_config_setting_aliases)

def _test_config_setting_aliases_many(env):
    # Use this function as it is used in pip_repository
    got = {}
    actual_no_match_error = []

    def mock_select(value, no_match_error = None):
        if no_match_error and no_match_error not in actual_no_match_error:
            actual_no_match_error.append(no_match_error)
        return value

    pkg_aliases(
        name = "bar_baz",
        actual = {
            (
                "//:my_config_setting",
                "//:another_config_setting",
            ): "bar_baz_repo",
            "//:third_config_setting": "foo_repo",
        },
        extra_aliases = ["my_special"],
        native = struct(
            alias = lambda *, name, actual, visibility = None, tags = None: got.update({name: actual}),
            config_setting = lambda **_: None,
        ),
        select = mock_select,
    )

    # buildifier: disable=unsorted-dict-items
    want = {
        "my_special": {
            (
                "//:my_config_setting",
                "//:another_config_setting",
            ): "@bar_baz_repo//:my_special",
            "//:third_config_setting": "@foo_repo//:my_special",
            "//conditions:default": "_no_matching_repository",
        },
    }
    env.expect.that_dict(got).contains_at_least(want)
    env.expect.that_collection(actual_no_match_error).has_size(1)
    env.expect.that_str(actual_no_match_error[0]).contains("""\
configuration settings:
    //:another_config_setting
    //:my_config_setting
    //:third_config_setting
""")

_tests.append(_test_config_setting_aliases_many)

def _test_multiplatform_whl_aliases(env):
    # Use this function as it is used in pip_repository
    got = {}
    actual_no_match_error = []

    def mock_select(value, no_match_error = None):
        if no_match_error and no_match_error not in actual_no_match_error:
            actual_no_match_error.append(no_match_error)
        return value

    pkg_aliases(
        name = "bar_baz",
        actual = {
            whl_config_setting(
                filename = "foo-0.0.0-py3-none-any.whl",
                version = "3.9",
            ): "filename_repo",
            whl_config_setting(
                filename = "foo-0.0.0-py3-none-any.whl",
                version = "3.9",
                target_platforms = ["cp39_linux_x86_64"],
            ): "filename_repo_for_platform",
            whl_config_setting(
                version = "3.9",
                target_platforms = ["cp39_linux_x86_64"],
            ): "bzlmod_repo_for_a_particular_platform",
            "//:my_config_setting": "bzlmod_repo",
        },
        extra_aliases = [],
        native = struct(
            alias = lambda *, name, actual, visibility = None, tags = None: got.update({name: actual}),
        ),
        select = mock_select,
        glibc_versions = [],
        muslc_versions = [],
        osx_versions = [],
    )

    # buildifier: disable=unsorted-dict-items
    want = {
        "pkg": {
            "//:my_config_setting": "@bzlmod_repo//:pkg",
            "//_config:is_cp39_linux_x86_64": "@bzlmod_repo_for_a_particular_platform//:pkg",
            "//_config:is_cp39_py3_none_any": "@filename_repo//:pkg",
            "//_config:is_cp39_py3_none_any_linux_x86_64": "@filename_repo_for_platform//:pkg",
            "//conditions:default": "_no_matching_repository",
        },
    }
    env.expect.that_dict(got).contains_at_least(want)
    env.expect.that_collection(actual_no_match_error).has_size(1)
    env.expect.that_str(actual_no_match_error[0]).contains("""\
configuration settings:
    //:my_config_setting
    //_config:is_cp39_linux_x86_64
    //_config:is_cp39_py3_none_any
    //_config:is_cp39_py3_none_any_linux_x86_64

""")

_tests.append(_test_multiplatform_whl_aliases)

def _test_group_aliases(env):
    # Use this function as it is used in pip_repository
    actual = []

    pkg_aliases(
        name = "foo",
        actual = "repo",
        group_name = "my_group",
        native = struct(
            alias = lambda **kwargs: actual.append(kwargs),
        ),
    )

    # buildifier: disable=unsorted-dict-items
    want = [
        {
            "name": "foo",
            "actual": ":pkg",
        },
        {
            "name": "_pkg",
            "actual": "@repo//:pkg",
            "visibility": ["//_groups:__subpackages__"],
        },
        {
            "name": "_whl",
            "actual": "@repo//:whl",
            "visibility": ["//_groups:__subpackages__"],
        },
        {
            "name": "data",
            "actual": "@repo//:data",
        },
        {
            "name": "dist_info",
            "actual": "@repo//:dist_info",
        },
        {
            "name": "pkg",
            "actual": "//_groups:my_group_pkg",
        },
        {
            "name": "whl",
            "actual": "//_groups:my_group_whl",
        },
    ]
    env.expect.that_collection(actual).contains_exactly(want)

_tests.append(_test_group_aliases)

def _test_multiplatform_whl_aliases_empty(env):
    # Check that we still work with an empty requirements.txt
    got = multiplatform_whl_aliases(aliases = {})
    env.expect.that_dict(got).contains_exactly({})

_tests.append(_test_multiplatform_whl_aliases_empty)

def _test_multiplatform_whl_aliases_nofilename(env):
    aliases = {
        "//:label": "foo",
    }
    got = multiplatform_whl_aliases(aliases = aliases)
    env.expect.that_dict(got).contains_exactly(aliases)

_tests.append(_test_multiplatform_whl_aliases_nofilename)

def _test_multiplatform_whl_aliases_nofilename_target_platforms(env):
    aliases = {
        whl_config_setting(
            config_setting = "//:ignored",
            version = "3.1",
            target_platforms = [
                "cp31_linux_x86_64",
                "cp31_linux_aarch64",
            ],
        ): "foo",
    }

    got = multiplatform_whl_aliases(aliases = aliases)

    want = {
        "//_config:is_cp31_linux_aarch64": "foo",
        "//_config:is_cp31_linux_x86_64": "foo",
    }
    env.expect.that_dict(got).contains_exactly(want)

_tests.append(_test_multiplatform_whl_aliases_nofilename_target_platforms)

def _test_multiplatform_whl_aliases_filename(env):
    aliases = {
        whl_config_setting(
            filename = "foo-0.0.3-py3-none-any.whl",
            version = "3.2",
        ): "foo-py3-0.0.3",
        whl_config_setting(
            filename = "foo-0.0.1-py3-none-any.whl",
            version = "3.1",
        ): "foo-py3-0.0.1",
        whl_config_setting(
            filename = "foo-0.0.1-cp313-cp313-any.whl",
            version = "3.13",
        ): "foo-cp-0.0.1",
        whl_config_setting(
            filename = "foo-0.0.1-cp313-cp313t-any.whl",
            version = "3.13",
        ): "foo-cpt-0.0.1",
        whl_config_setting(
            filename = "foo-0.0.2-py3-none-any.whl",
            version = "3.1",
            target_platforms = [
                "cp31_linux_x86_64",
                "cp31_linux_aarch64",
            ],
        ): "foo-0.0.2",
    }
    got = multiplatform_whl_aliases(
        aliases = aliases,
        glibc_versions = [],
        muslc_versions = [],
        osx_versions = [],
    )
    want = {
        "//_config:is_cp313_cp313_any": "foo-cp-0.0.1",
        "//_config:is_cp313_cp313t_any": "foo-cpt-0.0.1",
        "//_config:is_cp31_py3_none_any": "foo-py3-0.0.1",
        "//_config:is_cp31_py3_none_any_linux_aarch64": "foo-0.0.2",
        "//_config:is_cp31_py3_none_any_linux_x86_64": "foo-0.0.2",
        "//_config:is_cp32_py3_none_any": "foo-py3-0.0.3",
    }
    env.expect.that_dict(got).contains_exactly(want)

_tests.append(_test_multiplatform_whl_aliases_filename)

def _test_multiplatform_whl_aliases_filename_versioned(env):
    aliases = {
        whl_config_setting(
            filename = "foo-0.0.1-py3-none-manylinux_2_17_x86_64.whl",
            version = "3.1",
        ): "glibc-2.17",
        whl_config_setting(
            filename = "foo-0.0.1-py3-none-manylinux_2_18_x86_64.whl",
            version = "3.1",
        ): "glibc-2.18",
        whl_config_setting(
            filename = "foo-0.0.1-py3-none-musllinux_1_1_x86_64.whl",
            version = "3.1",
        ): "musl-1.1",
    }
    got = multiplatform_whl_aliases(
        aliases = aliases,
        glibc_versions = [(2, 17), (2, 18)],
        muslc_versions = [(1, 1), (1, 2)],
        osx_versions = [],
    )
    want = {
        # This could just work with:
        # select({
        #     "//_config:is_gt_eq_2.18": "//_config:is_cp3.1_py3_none_manylinux_x86_64",
        #     "//conditions:default": "//_config:is_gt_eq_2.18",
        # }): "glibc-2.18",
        # select({
        #     "//_config:is_range_2.17_2.18": "//_config:is_cp3.1_py3_none_manylinux_x86_64",
        #     "//_config:is_glibc_default": "//_config:is_cp3.1_py3_none_manylinux_x86_64",
        #     "//conditions:default": "//_config:is_glibc_default",
        # }): "glibc-2.17",
        # (
        #     "//_config:is_gt_musl_1.1":  "musl-1.1",
        #     "//_config:is_musl_default": "musl-1.1",
        # ): "musl-1.1",
        #
        # For this to fully work we need to have the pypi:config_settings.bzl to generate the
        # extra targets that use the FeatureFlagInfo and this to generate extra aliases for the
        # config settings.
        "//_config:is_cp31_py3_none_manylinux_2_17_x86_64": "glibc-2.17",
        "//_config:is_cp31_py3_none_manylinux_2_18_x86_64": "glibc-2.18",
        "//_config:is_cp31_py3_none_manylinux_x86_64": "glibc-2.17",
        "//_config:is_cp31_py3_none_musllinux_1_1_x86_64": "musl-1.1",
        "//_config:is_cp31_py3_none_musllinux_1_2_x86_64": "musl-1.1",
        "//_config:is_cp31_py3_none_musllinux_x86_64": "musl-1.1",
    }
    env.expect.that_dict(got).contains_exactly(want)

_tests.append(_test_multiplatform_whl_aliases_filename_versioned)

def _mock_alias(container):
    return lambda name, **kwargs: container.append(name)

def _mock_config_setting(container):
    def _inner(name, flag_values = None, constraint_values = None, **_):
        if flag_values or constraint_values:
            container.append(name)
            return

        fail("At least one of 'flag_values' or 'constraint_values' needs to be set")

    return _inner

def _test_config_settings_exist_legacy(env):
    aliases = {
        whl_config_setting(
            version = "3.11",
            target_platforms = [
                "cp311_linux_aarch64",
                "cp311_linux_x86_64",
            ],
        ): "repo",
    }
    available_config_settings = []
    config_settings(
        python_versions = ["3.11"],
        native = struct(
            alias = _mock_alias(available_config_settings),
            config_setting = _mock_config_setting(available_config_settings),
        ),
        target_platforms = [
            "linux_aarch64",
            "linux_x86_64",
        ],
    )

    got_aliases = multiplatform_whl_aliases(
        aliases = aliases,
    )
    got = [a.partition(":")[-1] for a in got_aliases]

    env.expect.that_collection(available_config_settings).contains_at_least(got)

_tests.append(_test_config_settings_exist_legacy)

def _test_config_settings_exist(env):
    for py_tag in ["py2.py3", "py3", "py311", "cp311"]:
        if py_tag == "py2.py3":
            abis = ["none"]
        elif py_tag.startswith("py"):
            abis = ["none", "abi3"]
        else:
            abis = ["none", "abi3", "cp311"]

        for abi_tag in abis:
            for platform_tag, kwargs in {
                "any": {},
                "macosx_11_0_arm64": {
                    "osx_versions": [(11, 0)],
                    "target_platforms": ["osx_aarch64"],
                },
                "manylinux_2_17_x86_64": {
                    "glibc_versions": [(2, 17), (2, 18)],
                    "target_platforms": ["linux_x86_64"],
                },
                "manylinux_2_18_x86_64": {
                    "glibc_versions": [(2, 17), (2, 18)],
                    "target_platforms": ["linux_x86_64"],
                },
                "musllinux_1_1_aarch64": {
                    "muslc_versions": [(1, 2), (1, 1), (1, 0)],
                    "target_platforms": ["linux_aarch64"],
                },
            }.items():
                aliases = {
                    whl_config_setting(
                        filename = "foo-0.0.1-{}-{}-{}.whl".format(py_tag, abi_tag, platform_tag),
                        version = "3.11",
                    ): "repo",
                }
                available_config_settings = []
                config_settings(
                    python_versions = ["3.11"],
                    native = struct(
                        alias = _mock_alias(available_config_settings),
                        config_setting = _mock_config_setting(available_config_settings),
                    ),
                    **kwargs
                )

                got_aliases = multiplatform_whl_aliases(
                    aliases = aliases,
                    glibc_versions = kwargs.get("glibc_versions", []),
                    muslc_versions = kwargs.get("muslc_versions", []),
                    osx_versions = kwargs.get("osx_versions", []),
                )
                got = [a.partition(":")[-1] for a in got_aliases]

                env.expect.that_collection(available_config_settings).contains_at_least(got)

_tests.append(_test_config_settings_exist)

def pkg_aliases_test_suite(name):
    """Create the test suite.

    Args:
        name: the name of the test suite
    """
    test_suite(name = name, basic_tests = _tests)
