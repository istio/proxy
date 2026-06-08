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
load("//python/private:common_labels.bzl", "labels")  # buildifier: disable=bzl-visibility
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
        "extracted_whl_files": "@repo//:extracted_whl_files",
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
        "_no_matching_repository": {Label("//python/config_settings:is_not_matching_current_config"): labels.NONE},
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
                version = "3.9",
            ): "version_repo",
            whl_config_setting(
                version = "3.9",
                target_platforms = ["cp39_linux_x86_64"],
            ): "version_platform_repo",
            "//:my_config_setting": "bzlmod_repo",
        },
        extra_aliases = [],
        native = struct(
            alias = lambda *, name, actual, visibility = None, tags = None: got.update({name: actual}),
        ),
        select = mock_select,
    )

    # buildifier: disable=unsorted-dict-items
    want = {
        "pkg": {
            "//:my_config_setting": "@bzlmod_repo//:pkg",
            "//_config:is_cp39": "@version_repo//:pkg",
            "//_config:is_cp39_linux_x86_64": "@version_platform_repo//:pkg",
            "//conditions:default": "_no_matching_repository",
        },
    }
    env.expect.that_dict(got).contains_at_least(want)
    env.expect.that_collection(actual_no_match_error).has_size(1)
    env.expect.that_str(actual_no_match_error[0]).contains("""\
configuration settings:
    //:my_config_setting
    //_config:is_cp39
    //_config:is_cp39_linux_x86_64

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
            "name": "extracted_whl_files",
            "actual": "@repo//:extracted_whl_files",
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

def _mock_alias(container):
    return lambda name, **kwargs: container.append(name)

def _mock_config_setting_group(container):
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
            config_setting = _mock_config_setting([]),
        ),
        selects = struct(
            config_setting_group = _mock_config_setting_group(available_config_settings),
        ),
        platform_config_settings = {
            "linux_aarch64": [
                "@platforms//cpu:aarch64",
                "@platforms//os:linux",
            ],
            "linux_x86_64": [
                "@platforms//cpu:x86_64",
                "@platforms//os:linux",
            ],
        },
    )

    got_aliases = multiplatform_whl_aliases(
        aliases = aliases,
    )
    got = [a.partition(":")[-1] for a in got_aliases]

    env.expect.that_collection(available_config_settings).contains_at_least(got)

_tests.append(_test_config_settings_exist_legacy)

def pkg_aliases_test_suite(name):
    """Create the test suite.

    Args:
        name: the name of the test suite
    """
    test_suite(name = name, basic_tests = _tests)
