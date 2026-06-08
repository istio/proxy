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
"""Tests common to py_binary and py_test (executable rules)."""

load("@rules_python//python:py_runtime_info.bzl", RulesPythonPyRuntimeInfo = "PyRuntimeInfo")
load("@rules_python_internal//:rules_python_config.bzl", rp_config = "config")
load("@rules_testing//lib:analysis_test.bzl", "analysis_test")
load("@rules_testing//lib:truth.bzl", "matching")
load("@rules_testing//lib:util.bzl", rt_util = "util")
load("//python:py_executable_info.bzl", "PyExecutableInfo")
load("//python:py_info.bzl", "PyInfo")
load("//python:py_library.bzl", "py_library")
load("//python/private:common.bzl", "maybe_builtin_build_python_zip")  # buildifier: disable=bzl-visibility
load("//python/private:common_labels.bzl", "labels")  # buildifier: disable=bzl-visibility
load("//python/private:reexports.bzl", "BuiltinPyRuntimeInfo")  # buildifier: disable=bzl-visibility
load("//tests/base_rules:base_tests.bzl", "create_base_tests")
load("//tests/base_rules:util.bzl", "WINDOWS_ATTR", pt_util = "util")
load("//tests/support:py_executable_info_subject.bzl", "PyExecutableInfoSubject")
load("//tests/support:support.bzl", "CC_TOOLCHAIN", "CROSSTOOL_TOP")
load("//tests/support/platforms:platforms.bzl", "platform_targets")

_tests = []

def _test_basic_windows(name, config):
    rt_util.helper_target(
        config.rule,
        name = name + "_subject",
        srcs = ["main.py"],
        main = "main.py",
    )
    analysis_test(
        name = name,
        impl = _test_basic_windows_impl,
        target = name + "_subject",
        config_settings = {
            # NOTE: The default for this flag is based on the Bazel host OS, not
            # the target platform. For windows, it defaults to true, so force
            # it to that to match behavior when this test runs on other
            # platforms.
            # Pass value to both native and starlark versions of the flag until
            # the native one is removed.
            labels.BUILD_PYTHON_ZIP: True,
            "//command_line_option:cpu": "windows_x86_64",
            "//command_line_option:crosstool_top": CROSSTOOL_TOP,
            "//command_line_option:extra_execution_platforms": [platform_targets.WINDOWS_X86_64],
            "//command_line_option:extra_toolchains": [CC_TOOLCHAIN],
            "//command_line_option:platforms": [platform_targets.WINDOWS_X86_64],
        } | maybe_builtin_build_python_zip("true"),
        attr_values = {},
    )

def _test_basic_windows_impl(env, target):
    target = env.expect.that_target(target)
    target.executable().path().contains(".exe")
    target.runfiles().contains_predicate(matching.str_endswith(
        target.meta.format_str("/{name}.zip"),
    ))
    target.runfiles().contains_predicate(matching.str_endswith(
        target.meta.format_str("/{name}.exe"),
    ))

_tests.append(_test_basic_windows)

def _test_basic_zip(name, config):
    target_compatible_with = select({
        # Disable the new test on windows because we have _test_basic_windows.
        "@platforms//os:windows": ["@platforms//:incompatible"],
        "//conditions:default": [],
    })
    rt_util.helper_target(
        config.rule,
        name = name + "_subject",
        srcs = ["main.py"],
        main = "main.py",
    )
    analysis_test(
        name = name,
        impl = _test_basic_zip_impl,
        target = name + "_subject",
        config_settings = {
            # NOTE: The default for this flag is based on the Bazel host OS, not
            # the target platform. For windows, it defaults to true, so force
            # it to that to match behavior when this test runs on other
            # platforms.
            # Pass value to both native and starlark versions of the flag until
            # the native one is removed.
            labels.BUILD_PYTHON_ZIP: True,
            "//command_line_option:cpu": "linux_x86_64",
            "//command_line_option:crosstool_top": CROSSTOOL_TOP,
            "//command_line_option:extra_execution_platforms": [platform_targets.LINUX_X86_64],
            "//command_line_option:extra_toolchains": [CC_TOOLCHAIN],
            "//command_line_option:platforms": [platform_targets.LINUX_X86_64],
        } | maybe_builtin_build_python_zip("true"),
        attr_values = {"target_compatible_with": target_compatible_with},
    )

def _test_basic_zip_impl(env, target):
    target = env.expect.that_target(target)
    target.runfiles().contains_predicate(matching.str_endswith(
        target.meta.format_str("/{name}.zip"),
    ))
    target.runfiles().contains_predicate(matching.str_endswith(
        target.meta.format_str("/{name}"),
    ))

_tests.append(_test_basic_zip)

def _test_cross_compile_to_unix(name, config):
    rt_util.helper_target(
        config.rule,
        name = name + "_subject",
        main_module = "dummy",
    )
    analysis_test(
        name = name,
        impl = _test_cross_compile_to_unix_impl,
        target = name + "_subject",
        # Cross-compilation of py_test fails since the default test toolchain
        # requires an execution platform that matches the target platform.
        config_settings = {
            "//command_line_option:platforms": [platform_targets.EXOTIC_UNIX],
        } if rp_config.bazel_9_or_later and not "py_test" in str(config.rule) else {},
        expect_failure = True,
    )

def _test_cross_compile_to_unix_impl(_env, _target):
    pass

_tests.append(_test_cross_compile_to_unix)

def _test_executable_in_runfiles(name, config):
    rt_util.helper_target(
        config.rule,
        name = name + "_subject",
        srcs = [name + "_subject.py"],
    )
    analysis_test(
        name = name,
        impl = _test_executable_in_runfiles_impl,
        target = name + "_subject",
        attrs = WINDOWS_ATTR,
    )

_tests.append(_test_executable_in_runfiles)

def _test_executable_in_runfiles_impl(env, target):
    if pt_util.is_windows(env):
        exe = ".exe"
    else:
        exe = ""
    env.expect.that_target(target).runfiles().contains_at_least([
        "{workspace}/{package}/{test_name}_subject" + exe,
    ])

    py_exec_info = env.expect.that_target(target).provider(PyExecutableInfo, factory = PyExecutableInfoSubject.new)
    py_exec_info.main().path().contains("_subject.py")
    py_exec_info.interpreter_path().contains("python")
    py_exec_info.runfiles_without_exe().contains_none_of([
        "{workspace}/{package}/{test_name}_subject" + exe,
        "{workspace}/{package}/{test_name}_subject",
    ])

def _test_debugger(name, config):
    # Using imports
    rt_util.helper_target(
        py_library,
        name = name + "_debugger",
        imports = ["."],
        srcs = [rt_util.empty_file(name + "_debugger.py")],
    )

    rt_util.helper_target(
        config.rule,
        name = name + "_subject",
        srcs = [rt_util.empty_file(name + "_subject.py")],
        config_settings = {
            # config_settings requires a fully qualified label
            labels.DEBUGGER: "//{}:{}_debugger".format(native.package_name(), name),
        },
    )

    # Using venv
    rt_util.helper_target(
        py_library,
        name = name + "_debugger_venv",
        imports = ["site-packages"],
        experimental_venvs_site_packages = "@rules_python//python/config_settings:venvs_site_packages",
        srcs = [rt_util.empty_file("site-packages/" + name + "_debugger_venv.py")],
    )

    rt_util.helper_target(
        config.rule,
        name = name + "_subject_venv",
        srcs = [rt_util.empty_file(name + "_subject_venv.py")],
        config_settings = {
            # config_settings requires a fully qualified label
            labels.DEBUGGER: "//{}:{}_debugger_venv".format(native.package_name(), name),
        },
    )

    analysis_test(
        name = name,
        impl = _test_debugger_impl,
        targets = {
            "exec_target": name + "_subject",
            "target": name + "_subject",
            "target_venv": name + "_subject_venv",
        },
        attrs = {
            "exec_target": attr.label(cfg = "exec"),
        },
        config_settings = {
            labels.VENVS_SITE_PACKAGES: "yes",
            labels.PYTHON_VERSION: "3.13",
        },
    )

_tests.append(_test_debugger)

def _test_debugger_impl(env, targets):
    # 1. Subject

    # Check the file from debugger dep is injected.
    env.expect.that_target(targets.target).runfiles().contains_at_least([
        "{workspace}/{package}/{test_name}_debugger.py",
    ])

    # #3481: Ensure imports are setup correcty.
    meta = env.expect.meta.derive(format_str_kwargs = {"package": targets.target.label.package})
    env.expect.that_target(targets.target).has_provider(PyInfo)
    imports = targets.target[PyInfo].imports.to_list()
    env.expect.that_collection(imports).contains(meta.format_str("{workspace}/{package}"))

    # 2. Subject venv

    # #3481: Ensure that venv site-packages is setup correctly, if the dependency is coming
    # from pip integration.
    env.expect.that_target(targets.target_venv).runfiles().contains_at_least([
        "{workspace}/{package}/_{name}.venv/lib/python3.13/site-packages/{test_name}_debugger_venv.py",
    ])

    # 3. Subject exec

    # Ensure that tools don't inherit debugger.
    env.expect.that_target(targets.exec_target).runfiles().not_contains(
        "{workspace}/{package}/{test_name}_debugger.py",
    )

def _test_default_main_can_be_generated(name, config):
    rt_util.helper_target(
        config.rule,
        name = name + "_subject",
        srcs = [rt_util.empty_file(name + "_subject.py")],
    )
    analysis_test(
        name = name,
        impl = _test_default_main_can_be_generated_impl,
        target = name + "_subject",
    )

_tests.append(_test_default_main_can_be_generated)

def _test_default_main_can_be_generated_impl(env, target):
    env.expect.that_target(target).default_outputs().contains(
        "{package}/{test_name}_subject.py",
    )

def _test_default_main_can_have_multiple_path_segments(name, config):
    rt_util.helper_target(
        config.rule,
        name = name + "/subject",
        srcs = [name + "/subject.py"],
    )
    analysis_test(
        name = name,
        impl = _test_default_main_can_have_multiple_path_segments_impl,
        target = name + "/subject",
    )

_tests.append(_test_default_main_can_have_multiple_path_segments)

def _test_default_main_can_have_multiple_path_segments_impl(env, target):
    env.expect.that_target(target).default_outputs().contains(
        "{package}/{test_name}/subject.py",
    )

def _test_default_main_must_be_in_srcs(name, config):
    rt_util.helper_target(
        config.rule,
        name = name + "_subject",
        srcs = ["other.py"],
    )
    analysis_test(
        name = name,
        impl = _test_default_main_must_be_in_srcs_impl,
        target = name + "_subject",
        expect_failure = True,
    )

_tests.append(_test_default_main_must_be_in_srcs)

def _test_default_main_must_be_in_srcs_impl(env, target):
    env.expect.that_target(target).failures().contains_predicate(
        matching.str_matches("default*does not appear in srcs"),
    )

def _test_default_main_cannot_be_ambiguous(name, config):
    rt_util.helper_target(
        config.rule,
        name = name + "_subject",
        srcs = [name + "_subject.py", "other/{}_subject.py".format(name)],
    )
    analysis_test(
        name = name,
        impl = _test_default_main_cannot_be_ambiguous_impl,
        target = name + "_subject",
        expect_failure = True,
    )

_tests.append(_test_default_main_cannot_be_ambiguous)

def _test_default_main_cannot_be_ambiguous_impl(env, target):
    env.expect.that_target(target).failures().contains_predicate(
        matching.str_matches("default main*matches multiple files"),
    )

def _test_explicit_main(name, config):
    rt_util.helper_target(
        config.rule,
        name = name + "_subject",
        srcs = ["custom.py"],
        main = "custom.py",
    )
    analysis_test(
        name = name,
        impl = _test_explicit_main_impl,
        target = name + "_subject",
    )

_tests.append(_test_explicit_main)

def _test_explicit_main_impl(env, target):
    # There isn't a direct way to ask what main file was selected, so we
    # rely on it being in the default outputs.
    env.expect.that_target(target).default_outputs().contains(
        "{package}/custom.py",
    )

def _test_explicit_main_cannot_be_ambiguous(name, config):
    rt_util.helper_target(
        config.rule,
        name = name + "_subject",
        srcs = ["x/foo.py", "y/foo.py"],
        main = "foo.py",
    )
    analysis_test(
        name = name,
        impl = _test_explicit_main_cannot_be_ambiguous_impl,
        target = name + "_subject",
        expect_failure = True,
    )

_tests.append(_test_explicit_main_cannot_be_ambiguous)

def _test_explicit_main_cannot_be_ambiguous_impl(env, target):
    env.expect.that_target(target).failures().contains_predicate(
        matching.str_matches("foo.py*matches multiple"),
    )

def _test_default_outputs(name, config):
    rt_util.helper_target(
        config.rule,
        name = name + "_subject",
        srcs = [name + "_subject.py"],
    )
    analysis_test(
        name = name,
        impl = _test_default_outputs_impl,
        target = name + "_subject",
        attrs = WINDOWS_ATTR,
    )

_tests.append(_test_default_outputs)

def _test_default_outputs_impl(env, target):
    default_outputs = env.expect.that_target(target).default_outputs()
    if pt_util.is_windows(env):
        default_outputs.contains("{package}/{test_name}_subject.exe")
    else:
        default_outputs.contains_exactly([
            "{package}/{test_name}_subject",
            "{package}/{test_name}_subject.py",
        ])

        # As of Bazel 7, the first default output is the executable, so
        # verify that is the case. rules_testing
        # DepsetFileSubject.contains_exactly doesn't provide an in_order()
        # call, nor access to the underlying depset, so we have to do things
        # manually.
        first_default_output = target[DefaultInfo].files.to_list()[0]
        executable = target[DefaultInfo].files_to_run.executable
        env.expect.that_file(first_default_output).equals(executable)

def _test_name_cannot_end_in_py(name, config):
    rt_util.helper_target(
        config.rule,
        name = name + "_subject.py",
        srcs = ["main.py"],
    )
    analysis_test(
        name = name,
        impl = _test_name_cannot_end_in_py_impl,
        target = name + "_subject.py",
        expect_failure = True,
    )

_tests.append(_test_name_cannot_end_in_py)

def _test_name_cannot_end_in_py_impl(env, target):
    env.expect.that_target(target).failures().contains_predicate(
        matching.str_matches("name must not end in*.py"),
    )

def _test_main_module_bootstrap_system_python(name, config):
    rt_util.helper_target(
        config.rule,
        name = name + "_subject",
        main_module = "dummy",
    )
    analysis_test(
        name = name,
        impl = _test_main_module_bootstrap_system_python_impl,
        target = name + "_subject",
        config_settings = {
            labels.BOOTSTRAP_IMPL: "system_python",
            "//command_line_option:extra_execution_platforms": ["@bazel_tools//tools:host_platform", platform_targets.LINUX_X86_64],
            "//command_line_option:platforms": [platform_targets.LINUX_X86_64],
        },
    )

def _test_main_module_bootstrap_system_python_impl(env, target):
    env.expect.that_target(target).default_outputs().contains(
        "{package}/{test_name}_subject",
    )

_tests.append(_test_main_module_bootstrap_system_python)

def _test_main_module_bootstrap_script(name, config):
    rt_util.helper_target(
        config.rule,
        name = name + "_subject",
        main_module = "dummy",
    )
    analysis_test(
        name = name,
        impl = _test_main_module_bootstrap_script_impl,
        target = name + "_subject",
        config_settings = {
            labels.BOOTSTRAP_IMPL: "script",
            "//command_line_option:extra_execution_platforms": ["@bazel_tools//tools:host_platform", platform_targets.LINUX_X86_64],
            "//command_line_option:platforms": [platform_targets.LINUX_X86_64],
        },
    )

def _test_main_module_bootstrap_script_impl(env, target):
    env.expect.that_target(target).default_outputs().contains(
        "{package}/{test_name}_subject",
    )

_tests.append(_test_main_module_bootstrap_script)

def _test_py_runtime_info_provided(name, config):
    rt_util.helper_target(
        config.rule,
        name = name + "_subject",
        srcs = [name + "_subject.py"],
    )
    analysis_test(
        name = name,
        impl = _test_py_runtime_info_provided_impl,
        target = name + "_subject",
    )

def _test_py_runtime_info_provided_impl(env, target):
    # Make sure that the rules_python loaded symbol is provided.
    env.expect.that_target(target).has_provider(RulesPythonPyRuntimeInfo)

    if BuiltinPyRuntimeInfo != None:
        # For compatibility during the transition, the builtin PyRuntimeInfo should
        # also be provided.
        env.expect.that_target(target).has_provider(BuiltinPyRuntimeInfo)

_tests.append(_test_py_runtime_info_provided)

# =====
# You were gonna add a test at the end, weren't you?
# Nope. Please keep them sorted; put it in its alphabetical location.
# Here's the alphabet so you don't have to sing that song in your head:
# A B C D E F G H I J K L M N O P Q R S T U V W X Y Z
# =====

def create_executable_tests(config):
    def _executable_with_srcs_wrapper(name, **kwargs):
        if not kwargs.get("srcs"):
            kwargs["srcs"] = [name + ".py"]
        config.rule(name = name, **kwargs)

    config = pt_util.struct_with(config, base_test_rule = _executable_with_srcs_wrapper)
    return pt_util.create_tests(_tests, config = config) + create_base_tests(config = config)
