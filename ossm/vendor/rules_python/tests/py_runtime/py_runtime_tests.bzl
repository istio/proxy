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
"""Starlark tests for py_runtime rule."""

load("@rules_python_internal//:rules_python_config.bzl", "config")
load("@rules_testing//lib:analysis_test.bzl", "analysis_test")
load("@rules_testing//lib:test_suite.bzl", "test_suite")
load("@rules_testing//lib:truth.bzl", "matching")
load("@rules_testing//lib:util.bzl", rt_util = "util")
load("//python:py_runtime.bzl", "py_runtime")
load("//python:py_runtime_info.bzl", "PyRuntimeInfo")
load("//tests/base_rules:util.bzl", br_util = "util")
load("//tests/support:py_runtime_info_subject.bzl", "py_runtime_info_subject")
load("//tests/support:support.bzl", "PYTHON_VERSION")

_tests = []

_SKIP_TEST = {
    "target_compatible_with": ["@platforms//:incompatible"],
}

def _simple_binary_impl(ctx):
    executable = ctx.actions.declare_file(ctx.label.name)
    ctx.actions.write(executable, "", is_executable = True)
    return [DefaultInfo(
        executable = executable,
        files = depset([executable] + ctx.files.extra_default_outputs),
        runfiles = ctx.runfiles(ctx.files.data),
    )]

_simple_binary = rule(
    implementation = _simple_binary_impl,
    attrs = {
        "data": attr.label_list(allow_files = True),
        "extra_default_outputs": attr.label_list(allow_files = True),
    },
    executable = True,
)

def _test_bootstrap_template(name):
    # The bootstrap_template arg isn't present in older Bazel versions, so
    # we have to conditionally pass the arg and mark the test incompatible.
    if config.enable_pystar:
        py_runtime_kwargs = {"bootstrap_template": "bootstrap.txt"}
        attr_values = {}
    else:
        py_runtime_kwargs = {}
        attr_values = _SKIP_TEST

    rt_util.helper_target(
        py_runtime,
        name = name + "_subject",
        interpreter_path = "/py",
        python_version = "PY3",
        **py_runtime_kwargs
    )
    analysis_test(
        name = name,
        target = name + "_subject",
        impl = _test_bootstrap_template_impl,
        attr_values = attr_values,
    )

def _test_bootstrap_template_impl(env, target):
    env.expect.that_target(target).provider(
        PyRuntimeInfo,
        factory = py_runtime_info_subject,
    ).bootstrap_template().path().contains("bootstrap.txt")

_tests.append(_test_bootstrap_template)

def _test_cannot_have_both_inbuild_and_system_interpreter(name):
    if br_util.is_bazel_6_or_higher():
        py_runtime_kwargs = {
            "interpreter": "fake_interpreter",
            "interpreter_path": "/some/path",
        }
        attr_values = {}
    else:
        py_runtime_kwargs = {
            "interpreter_path": "/some/path",
        }
        attr_values = _SKIP_TEST
    rt_util.helper_target(
        py_runtime,
        name = name + "_subject",
        python_version = "PY3",
        **py_runtime_kwargs
    )
    analysis_test(
        name = name,
        target = name + "_subject",
        impl = _test_cannot_have_both_inbuild_and_system_interpreter_impl,
        expect_failure = True,
        attr_values = attr_values,
    )

def _test_cannot_have_both_inbuild_and_system_interpreter_impl(env, target):
    env.expect.that_target(target).failures().contains_predicate(
        matching.str_matches("one of*interpreter*interpreter_path"),
    )

_tests.append(_test_cannot_have_both_inbuild_and_system_interpreter)

def _test_cannot_specify_files_for_system_interpreter(name):
    if br_util.is_bazel_6_or_higher():
        py_runtime_kwargs = {"files": ["foo.txt"]}
        attr_values = {}
    else:
        py_runtime_kwargs = {}
        attr_values = _SKIP_TEST
    rt_util.helper_target(
        py_runtime,
        name = name + "_subject",
        interpreter_path = "/foo",
        python_version = "PY3",
        **py_runtime_kwargs
    )
    analysis_test(
        name = name,
        target = name + "_subject",
        impl = _test_cannot_specify_files_for_system_interpreter_impl,
        expect_failure = True,
        attr_values = attr_values,
    )

def _test_cannot_specify_files_for_system_interpreter_impl(env, target):
    env.expect.that_target(target).failures().contains_predicate(
        matching.str_matches("files*must be empty"),
    )

_tests.append(_test_cannot_specify_files_for_system_interpreter)

def _test_coverage_tool_executable(name):
    if br_util.is_bazel_6_or_higher():
        py_runtime_kwargs = {
            "coverage_tool": name + "_coverage_tool",
        }
        attr_values = {}
    else:
        py_runtime_kwargs = {}
        attr_values = _SKIP_TEST

    rt_util.helper_target(
        py_runtime,
        name = name + "_subject",
        python_version = "PY3",
        interpreter_path = "/bogus",
        **py_runtime_kwargs
    )
    rt_util.helper_target(
        _simple_binary,
        name = name + "_coverage_tool",
        data = ["coverage_file1.txt", "coverage_file2.txt"],
    )
    analysis_test(
        name = name,
        target = name + "_subject",
        impl = _test_coverage_tool_executable_impl,
        attr_values = attr_values,
    )

def _test_coverage_tool_executable_impl(env, target):
    info = env.expect.that_target(target).provider(PyRuntimeInfo, factory = py_runtime_info_subject)
    info.coverage_tool().short_path_equals("{package}/{test_name}_coverage_tool")
    info.coverage_files().contains_exactly([
        "{package}/{test_name}_coverage_tool",
        "{package}/coverage_file1.txt",
        "{package}/coverage_file2.txt",
    ])

_tests.append(_test_coverage_tool_executable)

def _test_coverage_tool_plain_files(name):
    if br_util.is_bazel_6_or_higher():
        py_runtime_kwargs = {
            "coverage_tool": name + "_coverage_tool",
        }
        attr_values = {}
    else:
        py_runtime_kwargs = {}
        attr_values = _SKIP_TEST
    rt_util.helper_target(
        py_runtime,
        name = name + "_subject",
        python_version = "PY3",
        interpreter_path = "/bogus",
        **py_runtime_kwargs
    )
    rt_util.helper_target(
        native.filegroup,
        name = name + "_coverage_tool",
        srcs = ["coverage_tool.py"],
        data = ["coverage_file1.txt", "coverage_file2.txt"],
    )
    analysis_test(
        name = name,
        target = name + "_subject",
        impl = _test_coverage_tool_plain_files_impl,
        attr_values = attr_values,
    )

def _test_coverage_tool_plain_files_impl(env, target):
    info = env.expect.that_target(target).provider(PyRuntimeInfo, factory = py_runtime_info_subject)
    info.coverage_tool().short_path_equals("{package}/coverage_tool.py")
    info.coverage_files().contains_exactly([
        "{package}/coverage_tool.py",
        "{package}/coverage_file1.txt",
        "{package}/coverage_file2.txt",
    ])

_tests.append(_test_coverage_tool_plain_files)

def _test_in_build_interpreter(name):
    rt_util.helper_target(
        py_runtime,
        name = name + "_subject",
        interpreter = "fake_interpreter",
        python_version = "PY3",
        files = ["file1.txt"],
    )
    analysis_test(
        name = name,
        target = name + "_subject",
        impl = _test_in_build_interpreter_impl,
    )

def _test_in_build_interpreter_impl(env, target):
    info = env.expect.that_target(target).provider(PyRuntimeInfo, factory = py_runtime_info_subject)
    info.python_version().equals("PY3")
    info.files().contains_predicate(matching.file_basename_equals("file1.txt"))
    info.interpreter().path().contains("fake_interpreter")

_tests.append(_test_in_build_interpreter)

def _test_interpreter_binary_with_multiple_outputs(name):
    rt_util.helper_target(
        _simple_binary,
        name = name + "_built_interpreter",
        extra_default_outputs = ["extra_default_output.txt"],
        data = ["runfile.txt"],
    )

    rt_util.helper_target(
        py_runtime,
        name = name + "_subject",
        interpreter = name + "_built_interpreter",
        python_version = "PY3",
    )
    analysis_test(
        name = name,
        target = name + "_subject",
        impl = _test_interpreter_binary_with_multiple_outputs_impl,
    )

def _test_interpreter_binary_with_multiple_outputs_impl(env, target):
    target = env.expect.that_target(target)
    py_runtime_info = target.provider(
        PyRuntimeInfo,
        factory = py_runtime_info_subject,
    )
    py_runtime_info.interpreter().short_path_equals("{package}/{test_name}_built_interpreter")
    py_runtime_info.files().contains_exactly([
        "{package}/extra_default_output.txt",
        "{package}/runfile.txt",
        "{package}/{test_name}_built_interpreter",
    ])

    target.default_outputs().contains_exactly([
        "{package}/extra_default_output.txt",
        "{package}/runfile.txt",
        "{package}/{test_name}_built_interpreter",
    ])

    target.runfiles().contains_exactly([
        "{workspace}/{package}/runfile.txt",
        "{workspace}/{package}/{test_name}_built_interpreter",
    ])

_tests.append(_test_interpreter_binary_with_multiple_outputs)

def _test_interpreter_binary_with_single_output_and_runfiles(name):
    rt_util.helper_target(
        _simple_binary,
        name = name + "_built_interpreter",
        data = ["runfile.txt"],
    )

    rt_util.helper_target(
        py_runtime,
        name = name + "_subject",
        interpreter = name + "_built_interpreter",
        python_version = "PY3",
    )
    analysis_test(
        name = name,
        target = name + "_subject",
        impl = _test_interpreter_binary_with_single_output_and_runfiles_impl,
    )

def _test_interpreter_binary_with_single_output_and_runfiles_impl(env, target):
    target = env.expect.that_target(target)
    py_runtime_info = target.provider(
        PyRuntimeInfo,
        factory = py_runtime_info_subject,
    )
    py_runtime_info.interpreter().short_path_equals("{package}/{test_name}_built_interpreter")
    py_runtime_info.files().contains_exactly([
        "{package}/runfile.txt",
        "{package}/{test_name}_built_interpreter",
    ])

    target.default_outputs().contains_exactly([
        "{package}/runfile.txt",
        "{package}/{test_name}_built_interpreter",
    ])

    target.runfiles().contains_exactly([
        "{workspace}/{package}/runfile.txt",
        "{workspace}/{package}/{test_name}_built_interpreter",
    ])

_tests.append(_test_interpreter_binary_with_single_output_and_runfiles)

def _test_must_have_either_inbuild_or_system_interpreter(name):
    if br_util.is_bazel_6_or_higher():
        py_runtime_kwargs = {}
        attr_values = {}
    else:
        py_runtime_kwargs = {
            "interpreter_path": "/some/path",
        }
        attr_values = _SKIP_TEST
    rt_util.helper_target(
        py_runtime,
        name = name + "_subject",
        python_version = "PY3",
        **py_runtime_kwargs
    )
    analysis_test(
        name = name,
        target = name + "_subject",
        impl = _test_must_have_either_inbuild_or_system_interpreter_impl,
        expect_failure = True,
        attr_values = attr_values,
    )

def _test_must_have_either_inbuild_or_system_interpreter_impl(env, target):
    env.expect.that_target(target).failures().contains_predicate(
        matching.str_matches("one of*interpreter*interpreter_path"),
    )

_tests.append(_test_must_have_either_inbuild_or_system_interpreter)

def _test_system_interpreter(name):
    rt_util.helper_target(
        py_runtime,
        name = name + "_subject",
        interpreter_path = "/system/python",
        python_version = "PY3",
    )
    analysis_test(
        name = name,
        target = name + "_subject",
        impl = _test_system_interpreter_impl,
    )

def _test_system_interpreter_impl(env, target):
    env.expect.that_target(target).provider(
        PyRuntimeInfo,
        factory = py_runtime_info_subject,
    ).interpreter_path().equals("/system/python")

_tests.append(_test_system_interpreter)

def _test_system_interpreter_must_be_absolute(name):
    # Bazel 5.4 will entirely crash when an invalid interpreter_path
    # is given.
    if br_util.is_bazel_6_or_higher():
        py_runtime_kwargs = {"interpreter_path": "relative/path"}
        attr_values = {}
    else:
        py_runtime_kwargs = {"interpreter_path": "/junk/value/for/bazel5.4"}
        attr_values = _SKIP_TEST
    rt_util.helper_target(
        py_runtime,
        name = name + "_subject",
        python_version = "PY3",
        **py_runtime_kwargs
    )
    analysis_test(
        name = name,
        target = name + "_subject",
        impl = _test_system_interpreter_must_be_absolute_impl,
        expect_failure = True,
        attr_values = attr_values,
    )

def _test_system_interpreter_must_be_absolute_impl(env, target):
    env.expect.that_target(target).failures().contains_predicate(
        matching.str_matches("must be*absolute"),
    )

_tests.append(_test_system_interpreter_must_be_absolute)

def _interpreter_version_info_test(name, interpreter_version_info, impl, expect_failure = True):
    if config.enable_pystar:
        py_runtime_kwargs = {
            "interpreter_version_info": interpreter_version_info,
        }
        attr_values = {}
    else:
        py_runtime_kwargs = {}
        attr_values = _SKIP_TEST

    rt_util.helper_target(
        py_runtime,
        name = name + "_subject",
        python_version = "PY3",
        interpreter_path = "/py",
        **py_runtime_kwargs
    )
    analysis_test(
        name = name,
        target = name + "_subject",
        impl = impl,
        expect_failure = expect_failure,
        attr_values = attr_values,
    )

def _test_interpreter_version_info_must_define_major_and_minor_only_major(name):
    _interpreter_version_info_test(
        name,
        {
            "major": "3",
        },
        lambda env, target: (
            env.expect.that_target(target).failures().contains_predicate(
                matching.str_matches("must have at least two keys, 'major' and 'minor'"),
            )
        ),
    )

_tests.append(_test_interpreter_version_info_must_define_major_and_minor_only_major)

def _test_interpreter_version_info_must_define_major_and_minor_only_minor(name):
    _interpreter_version_info_test(
        name,
        {
            "minor": "3",
        },
        lambda env, target: (
            env.expect.that_target(target).failures().contains_predicate(
                matching.str_matches("must have at least two keys, 'major' and 'minor'"),
            )
        ),
    )

_tests.append(_test_interpreter_version_info_must_define_major_and_minor_only_minor)

def _test_interpreter_version_info_no_extraneous_keys(name):
    _interpreter_version_info_test(
        name,
        {
            "major": "3",
            "minor": "3",
            "something": "foo",
        },
        lambda env, target: (
            env.expect.that_target(target).failures().contains_predicate(
                matching.str_matches("unexpected keys [\"something\"]"),
            )
        ),
    )

_tests.append(_test_interpreter_version_info_no_extraneous_keys)

def _test_interpreter_version_info_sets_values_to_none_if_not_given(name):
    _interpreter_version_info_test(
        name,
        {
            "major": "3",
            "micro": "10",
            "minor": "3",
        },
        lambda env, target: (
            env.expect.that_target(target).provider(
                PyRuntimeInfo,
                factory = py_runtime_info_subject,
            ).interpreter_version_info().serial().equals(None)
        ),
        expect_failure = False,
    )

_tests.append(_test_interpreter_version_info_sets_values_to_none_if_not_given)

def _test_interpreter_version_info_parses_values_to_struct(name):
    _interpreter_version_info_test(
        name,
        {
            "major": "3",
            "micro": "10",
            "minor": "6",
            "releaselevel": "alpha",
            "serial": "1",
        },
        impl = _test_interpreter_version_info_parses_values_to_struct_impl,
        expect_failure = False,
    )

def _test_interpreter_version_info_parses_values_to_struct_impl(env, target):
    version_info = env.expect.that_target(target).provider(PyRuntimeInfo, factory = py_runtime_info_subject).interpreter_version_info()
    version_info.major().equals(3)
    version_info.minor().equals(6)
    version_info.micro().equals(10)
    version_info.releaselevel().equals("alpha")
    version_info.serial().equals(1)

_tests.append(_test_interpreter_version_info_parses_values_to_struct)

def _test_version_info_from_flag(name):
    if not config.enable_pystar:
        rt_util.skip_test(name)
        return
    py_runtime(
        name = name + "_subject",
        interpreter_version_info = None,
        interpreter_path = "/bogus",
    )
    analysis_test(
        name = name,
        target = name + "_subject",
        impl = _test_version_info_from_flag_impl,
        config_settings = {
            PYTHON_VERSION: "3.12",
        },
    )

def _test_version_info_from_flag_impl(env, target):
    version_info = env.expect.that_target(target).provider(PyRuntimeInfo, factory = py_runtime_info_subject).interpreter_version_info()
    version_info.major().equals(3)
    version_info.minor().equals(12)
    version_info.micro().equals(None)
    version_info.releaselevel().equals(None)
    version_info.serial().equals(None)

_tests.append(_test_version_info_from_flag)

def py_runtime_test_suite(name):
    test_suite(
        name = name,
        tests = _tests,
    )
