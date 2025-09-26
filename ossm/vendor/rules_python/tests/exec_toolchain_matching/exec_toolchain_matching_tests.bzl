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
"""Starlark tests for PyRuntimeInfo provider."""

load("@rules_testing//lib:analysis_test.bzl", "analysis_test")
load("@rules_testing//lib:test_suite.bzl", "test_suite")
load("@rules_testing//lib:util.bzl", rt_util = "util")
load("//python:py_runtime.bzl", "py_runtime")
load("//python:py_runtime_pair.bzl", "py_runtime_pair")
load("//python/private:toolchain_types.bzl", "EXEC_TOOLS_TOOLCHAIN_TYPE", "TARGET_TOOLCHAIN_TYPE")  # buildifier: disable=bzl-visibility
load("//python/private:util.bzl", "IS_BAZEL_7_OR_HIGHER")  # buildifier: disable=bzl-visibility
load("//tests/support:support.bzl", "LINUX", "MAC", "PYTHON_VERSION")

_LookupInfo = provider()  # buildifier: disable=provider-params

def _lookup_toolchains_impl(ctx):
    return [_LookupInfo(
        target = ctx.toolchains[TARGET_TOOLCHAIN_TYPE],
        exec = ctx.toolchains[EXEC_TOOLS_TOOLCHAIN_TYPE],
    )]

_lookup_toolchains = rule(
    implementation = _lookup_toolchains_impl,
    toolchains = [TARGET_TOOLCHAIN_TYPE, EXEC_TOOLS_TOOLCHAIN_TYPE],
    attrs = {"_use_auto_exec_groups": attr.bool(default = True)},
)

def define_py_runtime(name, **kwargs):
    py_runtime(
        name = name + "_runtime",
        **kwargs
    )
    py_runtime_pair(
        name = name,
        py3_runtime = name + "_runtime",
    )

_tests = []

def _test_exec_matches_target_python_version(name):
    rt_util.helper_target(
        _lookup_toolchains,
        name = name + "_subject",
    )

    # ==== Target toolchains =====

    # This is never matched. It comes first to ensure the python version
    # constraint is being respected.
    native.toolchain(
        name = "00_target_3.11_any",
        toolchain_type = TARGET_TOOLCHAIN_TYPE,
        toolchain = ":target_3.12_linux",
        target_settings = ["//python/config_settings:is_python_3.11"],
    )

    # This is matched by the top-level target being built in what --platforms
    # specifies.
    native.toolchain(
        name = "10_target_3.12_linux",
        toolchain_type = TARGET_TOOLCHAIN_TYPE,
        toolchain = ":target_3.12_linux",
        target_compatible_with = ["@platforms//os:linux"],
        target_settings = ["//python/config_settings:is_python_3.12"],
    )

    # This is matched when the exec config switches to the mac platform and
    # then looks for a Python runtime for itself.
    native.toolchain(
        name = "15_target_3.12_mac",
        toolchain_type = TARGET_TOOLCHAIN_TYPE,
        toolchain = ":target_3.12_mac",
        target_compatible_with = ["@platforms//os:macos"],
        target_settings = ["//python/config_settings:is_python_3.12"],
    )

    # This is never matched. It's just here so that toolchains from the
    # environment don't match.
    native.toolchain(
        name = "99_target_default",
        toolchain_type = TARGET_TOOLCHAIN_TYPE,
        toolchain = ":target_default",
    )

    # ==== Exec tools toolchains =====

    # Register a 3.11 before to ensure it the python version is respected
    native.toolchain(
        name = "00_exec_3.11_any",
        toolchain_type = EXEC_TOOLS_TOOLCHAIN_TYPE,
        toolchain = ":exec_3.11_any",
        target_settings = ["//python/config_settings:is_python_3.11"],
    )

    # Note that mac comes first. This is so it matches instead of linux
    # We only ever look for mac ones, so no need to register others.
    native.toolchain(
        name = "10_exec_3.12_mac",
        toolchain_type = EXEC_TOOLS_TOOLCHAIN_TYPE,
        toolchain = ":exec_3.12",
        exec_compatible_with = ["@platforms//os:macos"],
        target_settings = ["//python/config_settings:is_python_3.12"],
    )

    # This is never matched. It's just here so that toolchains from the
    # environment don't match.
    native.toolchain(
        name = "99_exec_default",
        toolchain_type = EXEC_TOOLS_TOOLCHAIN_TYPE,
        toolchain = ":exec_default",
    )

    analysis_test(
        name = name,
        target = name + "_subject",
        impl = _test_exec_matches_target_python_version_impl,
        config_settings = {
            "//command_line_option:extra_execution_platforms": [str(MAC)],
            "//command_line_option:extra_toolchains": ["//tests/exec_toolchain_matching:all"],
            "//command_line_option:platforms": [str(LINUX)],
            PYTHON_VERSION: "3.12",
        },
    )

_tests.append(_test_exec_matches_target_python_version)

def _test_exec_matches_target_python_version_impl(env, target):
    target_runtime = target[_LookupInfo].target.py3_runtime
    exec_runtime = target[_LookupInfo].exec.exec_tools.exec_interpreter[platform_common.ToolchainInfo].py3_runtime

    env.expect.that_str(target_runtime.interpreter_path).equals("/linux/python3.12")
    env.expect.that_str(exec_runtime.interpreter_path).equals("/mac/python3.12")

    if IS_BAZEL_7_OR_HIGHER:
        target_version = target_runtime.interpreter_version_info
        exec_version = exec_runtime.interpreter_version_info

        env.expect.that_bool(target_version == exec_version)

def exec_toolchain_matching_test_suite(name):
    test_suite(name = name, tests = _tests)
