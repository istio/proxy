# Copyright 2023 The Bazel Authors. All rights reserved.
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
"""Tests common to py_test, py_binary, and py_library rules."""

load("@rules_testing//lib:analysis_test.bzl", "analysis_test")
load("@rules_testing//lib:truth.bzl", "matching")
load("@rules_testing//lib:util.bzl", "PREVENT_IMPLICIT_BUILDING_TAGS", rt_util = "util")
load("//python:py_info.bzl", "PyInfo")
load("//python:py_library.bzl", "py_library")
load("//python/private:reexports.bzl", "BuiltinPyInfo")  # buildifier: disable=bzl-visibility
load("//tests/base_rules:util.bzl", pt_util = "util")
load("//tests/support:py_info_subject.bzl", "py_info_subject")

_tests = []

_PRODUCES_PY_INFO_ATTRS = {
    "imports": attr.string_list(),
    "srcs": attr.label_list(allow_files = True),
}

def _create_py_info(ctx, provider_type):
    return [provider_type(
        transitive_sources = depset(ctx.files.srcs),
        imports = depset(ctx.attr.imports),
    )]

def _produces_builtin_py_info_impl(ctx):
    return _create_py_info(ctx, BuiltinPyInfo)

_produces_builtin_py_info = rule(
    implementation = _produces_builtin_py_info_impl,
    attrs = _PRODUCES_PY_INFO_ATTRS,
)

def _produces_py_info_impl(ctx):
    return _create_py_info(ctx, PyInfo)

_produces_py_info = rule(
    implementation = _produces_py_info_impl,
    attrs = _PRODUCES_PY_INFO_ATTRS,
)

def _not_produces_py_info_impl(ctx):
    _ = ctx  # @unused
    return [DefaultInfo()]

_not_produces_py_info = rule(
    implementation = _not_produces_py_info_impl,
)

def _test_py_info_populated(name, config):
    rt_util.helper_target(
        config.base_test_rule,
        name = name + "_subject",
        srcs = [name + "_subject.py"],
        pyi_srcs = ["subject.pyi"],
        pyi_deps = [name + "_lib2"],
    )
    rt_util.helper_target(
        py_library,
        name = name + "_lib2",
        srcs = ["lib2.py"],
        pyi_srcs = ["lib2.pyi"],
    )

    analysis_test(
        name = name,
        target = name + "_subject",
        impl = _test_py_info_populated_impl,
    )

def _test_py_info_populated_impl(env, target):
    info = env.expect.that_target(target).provider(
        PyInfo,
        factory = py_info_subject,
    )
    info.direct_original_sources().contains_exactly([
        "{package}/test_py_info_populated_subject.py",
    ])
    info.transitive_original_sources().contains_exactly([
        "{package}/test_py_info_populated_subject.py",
        "{package}/lib2.py",
    ])

    info.direct_pyi_files().contains_exactly([
        "{package}/subject.pyi",
    ])
    info.transitive_pyi_files().contains_exactly([
        "{package}/lib2.pyi",
        "{package}/subject.pyi",
    ])

_tests.append(_test_py_info_populated)

def _py_info_propagation_setup(name, config, produce_py_info_rule, test_impl):
    rt_util.helper_target(
        config.base_test_rule,
        name = name + "_subject",
        deps = [name + "_produces_builtin_py_info"],
    )
    rt_util.helper_target(
        produce_py_info_rule,
        name = name + "_produces_builtin_py_info",
        srcs = [rt_util.empty_file(name + "_produce.py")],
        imports = ["custom-import"],
    )
    analysis_test(
        name = name,
        target = name + "_subject",
        impl = test_impl,
    )

def _py_info_propagation_test_impl(env, target, provider_type):
    info = env.expect.that_target(target).provider(
        provider_type,
        factory = py_info_subject,
    )

    info.transitive_sources().contains("{package}/{test_name}_produce.py")
    info.imports().contains("custom-import")

def _test_py_info_propagation_builtin(name, config):
    if not BuiltinPyInfo:
        rt_util.skip_test(name = name)
        return
    _py_info_propagation_setup(
        name,
        config,
        _produces_builtin_py_info,
        _test_py_info_propagation_builtin_impl,
    )

def _test_py_info_propagation_builtin_impl(env, target):
    _py_info_propagation_test_impl(env, target, BuiltinPyInfo)

_tests.append(_test_py_info_propagation_builtin)

def _test_py_info_propagation(name, config):
    _py_info_propagation_setup(
        name,
        config,
        _produces_py_info,
        _test_py_info_propagation_impl,
    )

def _test_py_info_propagation_impl(env, target):
    _py_info_propagation_test_impl(env, target, PyInfo)

_tests.append(_test_py_info_propagation)

def _test_requires_provider(name, config):
    rt_util.helper_target(
        config.base_test_rule,
        name = name + "_subject",
        deps = [name + "_nopyinfo"],
    )
    rt_util.helper_target(
        _not_produces_py_info,
        name = name + "_nopyinfo",
    )
    analysis_test(
        name = name,
        target = name + "_subject",
        impl = _test_requires_provider_impl,
        expect_failure = True,
    )

def _test_requires_provider_impl(env, target):
    env.expect.that_target(target).failures().contains_predicate(
        matching.str_matches("mandatory*PyInfo"),
    )

_tests.append(_test_requires_provider)

def _test_data_sets_uses_shared_library(name, config):
    rt_util.helper_target(
        config.base_test_rule,
        name = name + "_subject",
        data = [rt_util.empty_file(name + "_dso.so")],
    )
    analysis_test(
        name = name,
        target = name + "_subject",
        impl = _test_data_sets_uses_shared_library_impl,
    )

def _test_data_sets_uses_shared_library_impl(env, target):
    env.expect.that_target(target).provider(
        PyInfo,
        factory = py_info_subject,
    ).uses_shared_libraries().equals(True)

_tests.append(_test_data_sets_uses_shared_library)

def _test_tags_can_be_tuple(name, config):
    # We don't use a helper because we want to ensure that value passed is
    # a tuple.
    config.base_test_rule(
        name = name + "_subject",
        tags = ("one", "two") + tuple(PREVENT_IMPLICIT_BUILDING_TAGS),
    )
    analysis_test(
        name = name,
        target = name + "_subject",
        impl = _test_tags_can_be_tuple_impl,
    )

def _test_tags_can_be_tuple_impl(env, target):
    env.expect.that_target(target).tags().contains_at_least([
        "one",
        "two",
    ])

_tests.append(_test_tags_can_be_tuple)

def create_base_tests(config):
    return pt_util.create_tests(_tests, config = config)
