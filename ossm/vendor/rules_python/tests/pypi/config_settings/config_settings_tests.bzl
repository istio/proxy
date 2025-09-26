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
"""Tests for construction of Python version matching config settings."""

load("@rules_testing//lib:analysis_test.bzl", "analysis_test")
load("@rules_testing//lib:test_suite.bzl", "test_suite")
load("@rules_testing//lib:truth.bzl", "subjects")
load("@rules_testing//lib:util.bzl", test_util = "util")
load("//python/private/pypi:config_settings.bzl", "config_settings")  # buildifier: disable=bzl-visibility

def _subject_impl(ctx):
    _ = ctx  # @unused
    return [DefaultInfo()]

_subject = rule(
    implementation = _subject_impl,
    attrs = {
        "dist": attr.string(),
    },
)

_flag = struct(
    platform = lambda x: ("//command_line_option:platforms", str(Label("//tests/support:" + x))),
    pip_whl = lambda x: (str(Label("//python/config_settings:pip_whl")), str(x)),
    pip_whl_glibc_version = lambda x: (str(Label("//python/config_settings:pip_whl_glibc_version")), str(x)),
    pip_whl_muslc_version = lambda x: (str(Label("//python/config_settings:pip_whl_muslc_version")), str(x)),
    pip_whl_osx_version = lambda x: (str(Label("//python/config_settings:pip_whl_osx_version")), str(x)),
    pip_whl_osx_arch = lambda x: (str(Label("//python/config_settings:pip_whl_osx_arch")), str(x)),
    py_linux_libc = lambda x: (str(Label("//python/config_settings:py_linux_libc")), str(x)),
    python_version = lambda x: (str(Label("//python/config_settings:python_version")), str(x)),
    py_freethreaded = lambda x: (str(Label("//python/config_settings:py_freethreaded")), str(x)),
)

def _analysis_test(*, name, dist, want, config_settings = [_flag.platform("linux_aarch64")]):
    subject_name = name + "_subject"
    test_util.helper_target(
        _subject,
        name = subject_name,
        dist = select(
            dist | {
                "//conditions:default": "no_match",
            },
        ),
    )
    config_settings = dict(config_settings)
    if not config_settings:
        fail("For reproducibility on different platforms, the config setting must be specified")
    python_version, default_value = _flag.python_version("3.7.10")
    config_settings.setdefault(python_version, default_value)

    analysis_test(
        name = name,
        target = subject_name,
        impl = lambda env, target: _match(env, target, want),
        config_settings = config_settings,
    )

def _match(env, target, want):
    target = env.expect.that_target(target)
    target.attr("dist", factory = subjects.str).equals(want)

_tests = []

# Legacy pip config setting tests

def _test_legacy_default(name):
    _analysis_test(
        name = name,
        dist = {
            "is_cp37": "legacy",
        },
        want = "legacy",
    )

_tests.append(_test_legacy_default)

def _test_legacy_with_constraint_values(name):
    _analysis_test(
        name = name,
        dist = {
            "is_cp37": "legacy",
            "is_cp37_linux_aarch64": "legacy_platform_override",
        },
        want = "legacy_platform_override",
    )

_tests.append(_test_legacy_with_constraint_values)

def config_settings_test_suite(name):  # buildifier: disable=function-docstring
    test_suite(
        name = name,
        tests = _tests,
    )

    config_settings(
        name = "dummy",
        python_versions = ["3.7", "3.8", "3.9", "3.10"],
        platform_config_settings = {
            "linux_aarch64": [
                "@platforms//cpu:aarch64",
                "@platforms//os:linux",
            ],
            "linux_ppc": [
                "@platforms//cpu:ppc",
                "@platforms//os:linux",
            ],
            "linux_x86_64": [
                "@platforms//cpu:x86_64",
                "@platforms//os:linux",
            ],
            "osx_aarch64": [
                "@platforms//cpu:aarch64",
                "@platforms//os:osx",
            ],
            "osx_x86_64": [
                "@platforms//cpu:x86_64",
                "@platforms//os:osx",
            ],
            "windows_aarch64": [
                "@platforms//cpu:aarch64",
                "@platforms//os:windows",
            ],
            "windows_x86_64": [
                "@platforms//cpu:x86_64",
                "@platforms//os:windows",
            ],
        },
    )
