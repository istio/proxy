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

load("@rules_testing//lib:analysis_test.bzl", "analysis_test")
load("@rules_testing//lib:test_suite.bzl", "test_suite")
load("@rules_testing//lib:truth.bzl", "subjects")
load("@rules_testing//lib:util.bzl", rt_util = "util")
load("//python/private:builders.bzl", "builders")  # buildifier: disable=bzl-visibility

_tests = []

def _test_depset_builder(name):
    rt_util.helper_target(
        native.filegroup,
        name = name + "_files",
    )
    analysis_test(
        name = name,
        target = name + "_files",
        impl = _test_depset_builder_impl,
    )

def _test_depset_builder_impl(env, target):
    _ = target  # @unused
    builder = builders.DepsetBuilder()
    builder.set_order("preorder")
    builder.add("one")
    builder.add(["two"])
    builder.add(depset(["three"]))
    builder.add([depset(["four"])])

    env.expect.that_str(builder.get_order()).equals("preorder")

    actual = builder.build()

    env.expect.that_collection(actual).contains_exactly([
        "one",
        "two",
        "three",
        "four",
    ]).in_order()

_tests.append(_test_depset_builder)

def _test_runfiles_builder(name):
    rt_util.helper_target(
        native.filegroup,
        name = name + "_files",
        srcs = ["f1.txt", "f2.txt", "f3.txt", "f4.txt", "f5.txt"],
    )
    rt_util.helper_target(
        native.filegroup,
        name = name + "_runfiles",
        data = ["runfile.txt"],
    )
    analysis_test(
        name = name,
        impl = _test_runfiles_builder_impl,
        targets = {
            "files": name + "_files",
            "runfiles": name + "_runfiles",
        },
    )

def _test_runfiles_builder_impl(env, targets):
    ctx = env.ctx

    f1, f2, f3, f4, f5 = targets.files[DefaultInfo].files.to_list()
    builder = builders.RunfilesBuilder()
    builder.add(f1)
    builder.add([f2])
    builder.add(depset([f3]))

    rf1 = ctx.runfiles([f4])
    rf2 = ctx.runfiles([f5])
    builder.add(rf1)
    builder.add([rf2])

    builder.add_targets([targets.runfiles])

    builder.root_symlinks["root_link"] = f1
    builder.symlinks["regular_link"] = f1

    actual = builder.build(ctx)

    subject = subjects.runfiles(actual, meta = env.expect.meta)
    subject.contains_exactly([
        "root_link",
        "{workspace}/regular_link",
        "{workspace}/tests/builders/f1.txt",
        "{workspace}/tests/builders/f2.txt",
        "{workspace}/tests/builders/f3.txt",
        "{workspace}/tests/builders/f4.txt",
        "{workspace}/tests/builders/f5.txt",
        "{workspace}/tests/builders/runfile.txt",
    ])

_tests.append(_test_runfiles_builder)

def builders_test_suite(name):
    test_suite(
        name = name,
        tests = _tests,
    )
