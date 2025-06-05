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
"""Tests for py_info."""

load("@rules_python_internal//:rules_python_config.bzl", "config")
load("@rules_testing//lib:analysis_test.bzl", "analysis_test")
load("@rules_testing//lib:test_suite.bzl", "test_suite")
load("@rules_testing//lib:util.bzl", rt_util = "util")
load("//python:py_info.bzl", "PyInfo")
load("//python/private:py_info.bzl", "PyInfoBuilder")  # buildifier: disable=bzl-visibility
load("//python/private:reexports.bzl", "BuiltinPyInfo")  # buildifier: disable=bzl-visibility
load("//tests/support:py_info_subject.bzl", "py_info_subject")

def _provide_py_info_impl(ctx):
    kwargs = {
        "direct_original_sources": depset(ctx.files.direct_original_sources),
        "direct_pyc_files": depset(ctx.files.direct_pyc_files),
        "direct_pyi_files": depset(ctx.files.direct_pyi_files),
        "imports": depset(ctx.attr.imports),
        "transitive_original_sources": depset(ctx.files.transitive_original_sources),
        "transitive_pyc_files": depset(ctx.files.transitive_pyc_files),
        "transitive_pyi_files": depset(ctx.files.transitive_pyi_files),
        "transitive_sources": depset(ctx.files.transitive_sources),
    }
    if ctx.attr.has_py2_only_sources != -1:
        kwargs["has_py2_only_sources"] = bool(ctx.attr.has_py2_only_sources)
    if ctx.attr.has_py3_only_sources != -1:
        kwargs["has_py2_only_sources"] = bool(ctx.attr.has_py2_only_sources)

    providers = []
    if config.enable_pystar:
        providers.append(PyInfo(**kwargs))

    # Handle Bazel 6 or if Bazel autoloading is enabled
    if not config.enable_pystar or (BuiltinPyInfo and PyInfo != BuiltinPyInfo):
        providers.append(BuiltinPyInfo(**{
            k: kwargs[k]
            for k in (
                "transitive_sources",
                "has_py2_only_sources",
                "has_py3_only_sources",
                "uses_shared_libraries",
                "imports",
            )
            if k in kwargs
        }))
    return providers

provide_py_info = rule(
    implementation = _provide_py_info_impl,
    attrs = {
        "direct_original_sources": attr.label_list(allow_files = True),
        "direct_pyc_files": attr.label_list(allow_files = True),
        "direct_pyi_files": attr.label_list(allow_files = True),
        "has_py2_only_sources": attr.int(default = -1),
        "has_py3_only_sources": attr.int(default = -1),
        "imports": attr.string_list(),
        "transitive_original_sources": attr.label_list(allow_files = True),
        "transitive_pyc_files": attr.label_list(allow_files = True),
        "transitive_pyi_files": attr.label_list(allow_files = True),
        "transitive_sources": attr.label_list(allow_files = True),
    },
)

_tests = []

def _test_py_info_create(name):
    rt_util.helper_target(
        native.filegroup,
        name = name + "_files",
        srcs = ["trans.py", "direct.pyc", "trans.pyc"],
    )
    analysis_test(
        name = name,
        target = name + "_files",
        impl = _test_py_info_create_impl,
    )

def _test_py_info_create_impl(env, target):
    trans_py, direct_pyc, trans_pyc = target[DefaultInfo].files.to_list()
    actual = PyInfo(
        has_py2_only_sources = True,
        has_py3_only_sources = True,
        imports = depset(["import-path"]),
        transitive_sources = depset([trans_py]),
        uses_shared_libraries = True,
        **(dict(
            direct_pyc_files = depset([direct_pyc]),
            transitive_pyc_files = depset([trans_pyc]),
        ) if config.enable_pystar else {})
    )

    subject = py_info_subject(actual, meta = env.expect.meta)
    subject.uses_shared_libraries().equals(True)
    subject.has_py2_only_sources().equals(True)
    subject.has_py3_only_sources().equals(True)
    subject.transitive_sources().contains_exactly(["tests/base_rules/py_info/trans.py"])
    subject.imports().contains_exactly(["import-path"])
    if config.enable_pystar:
        subject.direct_pyc_files().contains_exactly(["tests/base_rules/py_info/direct.pyc"])
        subject.transitive_pyc_files().contains_exactly(["tests/base_rules/py_info/trans.pyc"])

_tests.append(_test_py_info_create)

def _test_py_info_builder(name):
    rt_util.helper_target(
        native.filegroup,
        name = name + "_misc",
        srcs = [
            "trans.py",
            "direct.pyc",
            "trans.pyc",
            "original.py",
            "trans-original.py",
            "direct.pyi",
            "trans.pyi",
        ],
    )

    py_info_targets = {}
    for n in range(1, 7):
        py_info_name = "{}_py{}".format(name, n)
        py_info_targets["py{}".format(n)] = py_info_name
        rt_util.helper_target(
            provide_py_info,
            name = py_info_name,
            transitive_sources = ["py{}-trans.py".format(n)],
            direct_pyc_files = ["py{}-direct.pyc".format(n)],
            imports = ["py{}import".format(n)],
            transitive_pyc_files = ["py{}-trans.pyc".format(n)],
            direct_original_sources = ["py{}-original-direct.py".format(n)],
            transitive_original_sources = ["py{}-original-trans.py".format(n)],
            direct_pyi_files = ["py{}-direct.pyi".format(n)],
            transitive_pyi_files = ["py{}-trans.pyi".format(n)],
        )
    analysis_test(
        name = name,
        impl = _test_py_info_builder_impl,
        targets = {
            "misc": name + "_misc",
        } | py_info_targets,
    )

def _test_py_info_builder_impl(env, targets):
    (
        trans,
        direct_pyc,
        trans_pyc,
        original_py,
        trans_original_py,
        direct_pyi,
        trans_pyi,
    ) = targets.misc[DefaultInfo].files.to_list()
    builder = PyInfoBuilder()
    builder.direct_pyc_files.add(direct_pyc)
    builder.direct_original_sources.add(original_py)
    builder.direct_pyi_files.add(direct_pyi)
    builder.merge_has_py2_only_sources(True)
    builder.merge_has_py3_only_sources(True)
    builder.imports.add("import-path")
    builder.transitive_pyc_files.add(trans_pyc)
    builder.transitive_pyi_files.add(trans_pyi)
    builder.transitive_original_sources.add(trans_original_py)
    builder.transitive_sources.add(trans)
    builder.merge_uses_shared_libraries(True)

    builder.merge_target(targets.py1)
    builder.merge_targets([targets.py2])

    builder.merge(targets.py3[PyInfo], direct = [targets.py4[PyInfo]])
    builder.merge_all([targets.py5[PyInfo]], direct = [targets.py6[PyInfo]])

    def check(actual):
        subject = py_info_subject(actual, meta = env.expect.meta)

        subject.uses_shared_libraries().equals(True)
        subject.has_py2_only_sources().equals(True)
        subject.has_py3_only_sources().equals(True)

        subject.transitive_sources().contains_exactly([
            "tests/base_rules/py_info/trans.py",
            "tests/base_rules/py_info/py1-trans.py",
            "tests/base_rules/py_info/py2-trans.py",
            "tests/base_rules/py_info/py3-trans.py",
            "tests/base_rules/py_info/py4-trans.py",
            "tests/base_rules/py_info/py5-trans.py",
            "tests/base_rules/py_info/py6-trans.py",
        ])
        subject.imports().contains_exactly([
            "import-path",
            "py1import",
            "py2import",
            "py3import",
            "py4import",
            "py5import",
            "py6import",
        ])

        # Checks for non-Bazel builtin PyInfo
        if hasattr(actual, "direct_pyc_files"):
            subject.direct_pyc_files().contains_exactly([
                "tests/base_rules/py_info/direct.pyc",
                "tests/base_rules/py_info/py4-direct.pyc",
                "tests/base_rules/py_info/py6-direct.pyc",
            ])
            subject.transitive_pyc_files().contains_exactly([
                "tests/base_rules/py_info/trans.pyc",
                "tests/base_rules/py_info/py1-trans.pyc",
                "tests/base_rules/py_info/py2-trans.pyc",
                "tests/base_rules/py_info/py3-trans.pyc",
                "tests/base_rules/py_info/py4-trans.pyc",
                "tests/base_rules/py_info/py5-trans.pyc",
                "tests/base_rules/py_info/py6-trans.pyc",
            ])
            subject.direct_original_sources().contains_exactly([
                "tests/base_rules/py_info/original.py",
                "tests/base_rules/py_info/py4-original-direct.py",
                "tests/base_rules/py_info/py6-original-direct.py",
            ])
            subject.transitive_original_sources().contains_exactly([
                "tests/base_rules/py_info/trans-original.py",
                "tests/base_rules/py_info/py1-original-trans.py",
                "tests/base_rules/py_info/py2-original-trans.py",
                "tests/base_rules/py_info/py3-original-trans.py",
                "tests/base_rules/py_info/py4-original-trans.py",
                "tests/base_rules/py_info/py5-original-trans.py",
                "tests/base_rules/py_info/py6-original-trans.py",
            ])
            subject.direct_pyi_files().contains_exactly([
                "tests/base_rules/py_info/direct.pyi",
                "tests/base_rules/py_info/py4-direct.pyi",
                "tests/base_rules/py_info/py6-direct.pyi",
            ])
            subject.transitive_pyi_files().contains_exactly([
                "tests/base_rules/py_info/trans.pyi",
                "tests/base_rules/py_info/py1-trans.pyi",
                "tests/base_rules/py_info/py2-trans.pyi",
                "tests/base_rules/py_info/py3-trans.pyi",
                "tests/base_rules/py_info/py4-trans.pyi",
                "tests/base_rules/py_info/py5-trans.pyi",
                "tests/base_rules/py_info/py6-trans.pyi",
            ])

    check(builder.build())
    if BuiltinPyInfo != None:
        check(builder.build_builtin_py_info())

    builder.set_has_py2_only_sources(False)
    builder.set_has_py3_only_sources(False)
    builder.set_uses_shared_libraries(False)

    env.expect.that_bool(builder.get_has_py2_only_sources()).equals(False)
    env.expect.that_bool(builder.get_has_py3_only_sources()).equals(False)
    env.expect.that_bool(builder.get_uses_shared_libraries()).equals(False)

_tests.append(_test_py_info_builder)

def py_info_test_suite(name):
    test_suite(
        name = name,
        tests = _tests,
    )
