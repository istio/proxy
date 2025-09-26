# Copyright 2024 The Bazel Authors. All rights reserved.
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
"""py_common tests."""

load("@rules_python_internal//:rules_python_config.bzl", "config")
load("@rules_testing//lib:analysis_test.bzl", "analysis_test")
load("@rules_testing//lib:test_suite.bzl", "test_suite")
load("@rules_testing//lib:util.bzl", rt_util = "util")
load("//python/api:api.bzl", _py_common = "py_common")
load("//tests/support:py_info_subject.bzl", "py_info_subject")

_tests = []

def _test_merge_py_infos(name):
    rt_util.helper_target(
        native.filegroup,
        name = name + "_subject",
        srcs = ["f1.py", "f1.pyc", "f2.py", "f2.pyc"],
    )
    analysis_test(
        name = name,
        impl = _test_merge_py_infos_impl,
        target = name + "_subject",
        attrs = _py_common.API_ATTRS,
    )

def _test_merge_py_infos_impl(env, target):
    f1_py, f1_pyc, f2_py, f2_pyc = target[DefaultInfo].files.to_list()

    py_common = _py_common.get(env.ctx)

    py1 = py_common.PyInfoBuilder()
    if config.enable_pystar:
        py1.direct_pyc_files.add(f1_pyc)
    py1.transitive_sources.add(f1_py)

    py2 = py_common.PyInfoBuilder()
    if config.enable_pystar:
        py1.direct_pyc_files.add(f2_pyc)
    py2.transitive_sources.add(f2_py)

    actual = py_info_subject(
        py_common.merge_py_infos([py2.build()], direct = [py1.build()]),
        meta = env.expect.meta,
    )

    actual.transitive_sources().contains_exactly([f1_py.path, f2_py.path])
    if config.enable_pystar:
        actual.direct_pyc_files().contains_exactly([f1_pyc.path, f2_pyc.path])

_tests.append(_test_merge_py_infos)

def py_common_test_suite(name):
    test_suite(
        name = name,
        tests = _tests,
    )
