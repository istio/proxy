# Copyright 2025 The Bazel Authors. All rights reserved.
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
"""Tests for construction of Python version matching config settings."""

load("@rules_testing//lib:test_suite.bzl", "test_suite")
load("//python/private/pypi:pep508_deps.bzl", "deps")  # buildifier: disable=bzl-visibility

_tests = []

def test_simple_deps(env):
    got = deps(
        "foo",
        requires_dist = ["bar-Bar"],
    )
    env.expect.that_collection(got.deps).contains_exactly(["bar_bar"])
    env.expect.that_dict(got.deps_select).contains_exactly({})

_tests.append(test_simple_deps)

def test_can_add_os_specific_deps(env):
    got = deps(
        "foo",
        requires_dist = [
            "bar",
            "an_osx_dep; sys_platform=='darwin'",
            "posix_dep; os_name=='posix'",
            "win_dep; os_name=='nt'",
        ],
    )

    env.expect.that_collection(got.deps).contains_exactly(["bar"])
    env.expect.that_dict(got.deps_select).contains_exactly({
        "an_osx_dep": "sys_platform == \"darwin\"",
        "posix_dep": "os_name == \"posix\"",
        "win_dep": "os_name == \"nt\"",
    })

_tests.append(test_can_add_os_specific_deps)

def test_deps_are_added_to_more_specialized_platforms(env):
    got = deps(
        "foo",
        requires_dist = [
            "m1_dep; sys_platform=='darwin' and platform_machine=='arm64'",
            "mac_dep; sys_platform=='darwin'",
        ],
    )

    env.expect.that_collection(got.deps).contains_exactly([])
    env.expect.that_dict(got.deps_select).contains_exactly({
        "m1_dep": "sys_platform == \"darwin\" and platform_machine == \"arm64\"",
        "mac_dep": "sys_platform == \"darwin\"",
    })

_tests.append(test_deps_are_added_to_more_specialized_platforms)

def test_self_is_ignored(env):
    got = deps(
        "foo",
        requires_dist = [
            "bar",
            "req_dep; extra == 'requests'",
            "foo[requests]; extra == 'ssl'",
            "ssl_lib; extra == 'ssl'",
        ],
        extras = ["ssl"],
    )

    env.expect.that_collection(got.deps).contains_exactly(["bar", "req_dep", "ssl_lib"])
    env.expect.that_dict(got.deps_select).contains_exactly({})

_tests.append(test_self_is_ignored)

def test_self_dependencies_can_come_in_any_order(env):
    got = deps(
        "foo",
        requires_dist = [
            "bar",
            "baz; extra == 'feat'",
            "foo[feat2]; extra == 'all'",
            "foo[feat]; extra == 'feat2'",
            "foo[feat3]; extra == 'all'",
            "zdep; extra == 'all'",
        ],
        extras = ["all"],
    )

    env.expect.that_collection(got.deps).contains_exactly(["bar", "baz", "zdep"])
    env.expect.that_dict(got.deps_select).contains_exactly({})

_tests.append(test_self_dependencies_can_come_in_any_order)

def test_self_include_deps_from_previously_visited(env):
    got = deps(
        "foo",
        requires_dist = [
            "bar",
            "baz; extra == 'feat'",
            "foo[dev]; extra == 'all'",
            "foo[feat]; extra == 'feat2'",
            "dev_dep; extra == 'dev'",
        ],
        extras = ["feat2"],
    )

    env.expect.that_collection(got.deps).contains_exactly(["bar", "baz"])
    env.expect.that_dict(got.deps_select).contains_exactly({})

_tests.append(test_self_include_deps_from_previously_visited)

def _test_can_get_deps_based_on_specific_python_version(env):
    requires_dist = [
        "bar",
        "baz; python_full_version < '3.7.3'",
        "posix_dep; os_name=='posix' and python_version >= '3.8'",
    ]

    got = deps(
        "foo",
        requires_dist = requires_dist,
    )

    # since there is a single target platform, the deps_select will be empty
    env.expect.that_collection(got.deps).contains_exactly(["bar"])
    env.expect.that_dict(got.deps_select).contains_exactly({
        "baz": "python_full_version < \"3.7.3\"",
        "posix_dep": "os_name == \"posix\" and python_version >= \"3.8\"",
    })

_tests.append(_test_can_get_deps_based_on_specific_python_version)

def _test_include_only_particular_deps(env):
    requires_dist = [
        "bar",
        "baz; python_full_version < '3.7.3'",
        "posix_dep; os_name=='posix' and python_version >= '3.8'",
    ]

    got = deps(
        "foo",
        requires_dist = requires_dist,
        include = ["bar", "posix_dep"],
    )

    # since there is a single target platform, the deps_select will be empty
    env.expect.that_collection(got.deps).contains_exactly(["bar"])
    env.expect.that_dict(got.deps_select).contains_exactly({
        "posix_dep": "os_name == \"posix\" and python_version >= \"3.8\"",
    })

_tests.append(_test_include_only_particular_deps)

def test_all_markers_are_added(env):
    requires_dist = [
        "bar",
        "baz (<2,>=1.11) ; python_version < '3.8'",
        "baz (<2,>=1.14) ; python_version >= '3.8'",
    ]

    got = deps(
        "foo",
        requires_dist = requires_dist,
    )

    env.expect.that_collection(got.deps).contains_exactly(["bar"])
    env.expect.that_dict(got.deps_select).contains_exactly({
        "baz": "(python_version < \"3.8\") or (python_version >= \"3.8\")",
    })

_tests.append(test_all_markers_are_added)

def test_extra_with_conditional_and_unconditional_markers(env):
    requires_dist = [
        "bar",
        'baz!=1.56.0; sys_platform == "darwin" and extra == "client"',
        'baz; extra == "client"',
    ]

    got = deps(
        "foo",
        extras = ["client"],
        requires_dist = requires_dist,
    )

    env.expect.that_collection(got.deps).contains_exactly(["bar", "baz"])
    env.expect.that_dict(got.deps_select).contains_exactly({})

_tests.append(test_extra_with_conditional_and_unconditional_markers)

def test_span_all_python_versions(env):
    requires_dist = [
        "bar>=0.4.0; python_version >= \"3.13.0\"",
        "bar>=0.3.0; python_version ~= \"3.12.0\"",
        "bar>=0.2.0; python_version ~= \"3.11.0\"",
        "bar>=0.1.0; python_version < \"3.11\"",
    ]

    got = deps(
        "foo",
        requires_dist = requires_dist,
    )

    env.expect.that_collection(got.deps).contains_exactly([])
    env.expect.that_dict(got.deps_select).contains_exactly({
        "bar": "(python_version < \"3.11\") or (python_version >= \"3.13.0\") or (python_version ~= \"3.11.0\") or (python_version ~= \"3.12.0\")",
    })

_tests.append(test_span_all_python_versions)

def test_extras_with_hyphens_are_normalized(env):
    """Test that extras with hyphens in marker expressions are normalized.

    When wheel METADATA uses hyphens in marker expressions
    (e.g., extra == "db-backend") but the extras from requirement parsing
    are already normalized (e.g., "db_backend"), the deps should still
    resolve because marker evaluation normalizes per PEP 685.

    Args:
        env: the test environment.
    """
    requires_dist = [
        "bar",
        'baz-lib; extra == "db-backend"',
        'qux-async; extra == "async-driver"',
    ]

    got = deps(
        "foo",
        extras = ["db_backend", "async_driver"],
        requires_dist = requires_dist,
    )

    env.expect.that_collection(got.deps).contains_exactly([
        "bar",
        "baz_lib",
        "qux_async",
    ])
    env.expect.that_dict(got.deps_select).contains_exactly({})

_tests.append(test_extras_with_hyphens_are_normalized)

def deps_test_suite(name):  # buildifier: disable=function-docstring
    test_suite(
        name = name,
        basic_tests = _tests,
    )
