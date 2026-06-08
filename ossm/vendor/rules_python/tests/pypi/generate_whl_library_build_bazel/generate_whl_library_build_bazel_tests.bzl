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

""

load("@rules_testing//lib:test_suite.bzl", "test_suite")
load("//python/private/pypi:generate_whl_library_build_bazel.bzl", "generate_whl_library_build_bazel")  # buildifier: disable=bzl-visibility

_tests = []

def _test_all_legacy(env):
    want = """\
load("@rules_python//python/private/pypi:whl_library_targets.bzl", "whl_library_targets")

package(default_visibility = ["//visibility:public"])

whl_library_targets(
    copy_executables = {
        "exec_src": "exec_dest",
    },
    copy_files = {
        "file_src": "file_dest",
    },
    data = ["extra_target"],
    data_exclude = [
        "exclude_via_attr",
        "data_exclude_all",
    ],
    dep_template = "@pypi_{name}//:{target}",
    dependencies = ["foo"],
    dependencies_by_platform = {
        "baz": ["bar"],
    },
    entry_points = {
        "foo": "bar.py",
    },
    group_deps = [
        "foo",
        "fox",
        "qux",
    ],
    group_name = "qux",
    name = "foo.whl",
    srcs_exclude = ["srcs_exclude_all"],
    tags = ["tag1"],
)

exports_files(
    srcs = ["bar.py"],
    visibility = ["//visibility:public"],
)

# SOMETHING SPECIAL AT THE END
"""
    actual = generate_whl_library_build_bazel(
        dep_template = "@pypi_{name}//:{target}",
        name = "foo.whl",
        dependencies = ["foo"],
        dependencies_by_platform = {"baz": ["bar"]},
        entry_points = {
            "foo": "bar.py",
        },
        data_exclude = ["exclude_via_attr"],
        annotation = struct(
            copy_files = {"file_src": "file_dest"},
            copy_executables = {"exec_src": "exec_dest"},
            data = ["extra_target"],
            data_exclude_glob = ["data_exclude_all"],
            srcs_exclude_glob = ["srcs_exclude_all"],
            additive_build_content = """# SOMETHING SPECIAL AT THE END""",
        ),
        group_name = "qux",
        group_deps = ["foo", "fox", "qux"],
        tags = ["tag1"],
    )
    env.expect.that_str(actual.replace("@@", "@")).equals(want)

_tests.append(_test_all_legacy)

def _test_all_workspace(env):
    want = """\
load("@pypi//:config.bzl", "packages")
load("@rules_python//python/private/pypi:whl_library_targets.bzl", "whl_library_targets_from_requires")

package(default_visibility = ["//visibility:public"])

whl_library_targets_from_requires(
    copy_executables = {
        "exec_src": "exec_dest",
    },
    copy_files = {
        "file_src": "file_dest",
    },
    data = ["extra_target"],
    data_exclude = [
        "exclude_via_attr",
        "data_exclude_all",
    ],
    dep_template = "@pypi//{name}:{target}",
    entry_points = {
        "foo": "bar.py",
    },
    group_deps = [
        "foo",
        "fox",
        "qux",
    ],
    group_name = "qux",
    include = packages,
    name = "foo.whl",
    requires_dist = [
        "foo",
        "bar-baz",
        "qux",
    ],
    srcs_exclude = ["srcs_exclude_all"],
)

exports_files(
    srcs = ["bar.py"],
    visibility = ["//visibility:public"],
)

# SOMETHING SPECIAL AT THE END
"""
    actual = generate_whl_library_build_bazel(
        dep_template = "@pypi//{name}:{target}",
        name = "foo.whl",
        requires_dist = ["foo", "bar-baz", "qux"],
        entry_points = {
            "foo": "bar.py",
        },
        data_exclude = ["exclude_via_attr"],
        annotation = struct(
            copy_files = {"file_src": "file_dest"},
            copy_executables = {"exec_src": "exec_dest"},
            data = ["extra_target"],
            data_exclude_glob = ["data_exclude_all"],
            srcs_exclude_glob = ["srcs_exclude_all"],
            additive_build_content = """# SOMETHING SPECIAL AT THE END""",
        ),
        config_load = "@pypi//:config.bzl",
        group_name = "qux",
        group_deps = ["foo", "fox", "qux"],
    )
    env.expect.that_str(actual.replace("@@", "@")).equals(want)

_tests.append(_test_all_workspace)

def _test_all(env):
    want = """\
load("@pypi//:config.bzl", "packages")
load("@rules_python//python/private/pypi:whl_library_targets.bzl", "whl_library_targets_from_requires")

package(default_visibility = ["//visibility:public"])

whl_library_targets_from_requires(
    copy_executables = {
        "exec_src": "exec_dest",
    },
    copy_files = {
        "file_src": "file_dest",
    },
    data = ["extra_target"],
    data_exclude = [
        "exclude_via_attr",
        "data_exclude_all",
    ],
    dep_template = "@pypi//{name}:{target}",
    entry_points = {
        "foo": "bar.py",
    },
    group_deps = [
        "foo",
        "fox",
        "qux",
    ],
    group_name = "qux",
    include = packages,
    name = "foo.whl",
    requires_dist = [
        "foo",
        "bar-baz",
        "qux",
    ],
    srcs_exclude = ["srcs_exclude_all"],
)

exports_files(
    srcs = ["bar.py"],
    visibility = ["//visibility:public"],
)

# SOMETHING SPECIAL AT THE END
"""
    actual = generate_whl_library_build_bazel(
        dep_template = "@pypi//{name}:{target}",
        name = "foo.whl",
        requires_dist = ["foo", "bar-baz", "qux"],
        entry_points = {
            "foo": "bar.py",
        },
        data_exclude = ["exclude_via_attr"],
        annotation = struct(
            copy_files = {"file_src": "file_dest"},
            copy_executables = {"exec_src": "exec_dest"},
            data = ["extra_target"],
            data_exclude_glob = ["data_exclude_all"],
            srcs_exclude_glob = ["srcs_exclude_all"],
            additive_build_content = """# SOMETHING SPECIAL AT THE END""",
        ),
        config_load = "@pypi//:config.bzl",
        group_name = "qux",
        group_deps = ["foo", "fox", "qux"],
    )
    env.expect.that_str(actual.replace("@@", "@")).equals(want)

_tests.append(_test_all)

def _test_all_with_loads(env):
    want = """\
load("@pypi//:config.bzl", "packages")
load("@rules_python//python/private/pypi:whl_library_targets.bzl", "whl_library_targets_from_requires")

package(default_visibility = ["//visibility:public"])

whl_library_targets_from_requires(
    copy_executables = {
        "exec_src": "exec_dest",
    },
    copy_files = {
        "file_src": "file_dest",
    },
    data = ["extra_target"],
    data_exclude = [
        "exclude_via_attr",
        "data_exclude_all",
    ],
    dep_template = "@pypi//{name}:{target}",
    entry_points = {
        "foo": "bar.py",
    },
    group_deps = [
        "foo",
        "fox",
        "qux",
    ],
    group_name = "qux",
    include = packages,
    name = "foo.whl",
    requires_dist = [
        "foo",
        "bar-baz",
        "qux",
    ],
    srcs_exclude = ["srcs_exclude_all"],
)

exports_files(
    srcs = ["bar.py"],
    visibility = ["//visibility:public"],
)

# SOMETHING SPECIAL AT THE END
"""
    actual = generate_whl_library_build_bazel(
        dep_template = "@pypi//{name}:{target}",
        name = "foo.whl",
        requires_dist = ["foo", "bar-baz", "qux"],
        entry_points = {
            "foo": "bar.py",
        },
        data_exclude = ["exclude_via_attr"],
        annotation = struct(
            copy_files = {"file_src": "file_dest"},
            copy_executables = {"exec_src": "exec_dest"},
            data = ["extra_target"],
            data_exclude_glob = ["data_exclude_all"],
            srcs_exclude_glob = ["srcs_exclude_all"],
            additive_build_content = """# SOMETHING SPECIAL AT THE END""",
        ),
        group_name = "qux",
        config_load = "@pypi//:config.bzl",
        group_deps = ["foo", "fox", "qux"],
    )
    env.expect.that_str(actual.replace("@@", "@")).equals(want)

_tests.append(_test_all_with_loads)

def generate_whl_library_build_bazel_test_suite(name):
    """Create the test suite.

    Args:
        name: the name of the test suite
    """
    test_suite(name = name, basic_tests = _tests)
