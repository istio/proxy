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

# Tests when we only have an `sdist` present.

def _test_sdist_default(name):
    _analysis_test(
        name = name,
        dist = {
            "is_cp37_sdist": "sdist",
        },
        want = "sdist",
    )

_tests.append(_test_sdist_default)

def _test_legacy_less_specialized_than_sdist(name):
    _analysis_test(
        name = name,
        dist = {
            "is_cp37": "legacy",
            "is_cp37_sdist": "sdist",
        },
        want = "sdist",
    )

_tests.append(_test_legacy_less_specialized_than_sdist)

def _test_sdist_no_whl(name):
    _analysis_test(
        name = name,
        dist = {
            "is_cp37_sdist": "sdist",
        },
        config_settings = [
            _flag.platform("linux_aarch64"),
            _flag.pip_whl("no"),
        ],
        want = "sdist",
    )

_tests.append(_test_sdist_no_whl)

def _test_sdist_no_sdist(name):
    _analysis_test(
        name = name,
        dist = {
            "is_cp37_sdist": "sdist",
        },
        config_settings = [
            _flag.platform("linux_aarch64"),
            _flag.pip_whl("only"),
        ],
        # We will use `no_match_error` in the real case to indicate that `sdist` is not
        # allowed to be used.
        want = "no_match",
    )

_tests.append(_test_sdist_no_sdist)

def _test_basic_whl_default(name):
    _analysis_test(
        name = name,
        dist = {
            "is_cp37_py_none_any": "whl",
            "is_cp37_sdist": "sdist",
        },
        want = "whl",
    )

_tests.append(_test_basic_whl_default)

def _test_basic_whl_nowhl(name):
    _analysis_test(
        name = name,
        dist = {
            "is_cp37_py_none_any": "whl",
            "is_cp37_sdist": "sdist",
        },
        config_settings = [
            _flag.platform("linux_aarch64"),
            _flag.pip_whl("no"),
        ],
        want = "sdist",
    )

_tests.append(_test_basic_whl_nowhl)

def _test_basic_whl_nosdist(name):
    _analysis_test(
        name = name,
        dist = {
            "is_cp37_py_none_any": "whl",
            "is_cp37_sdist": "sdist",
        },
        config_settings = [
            _flag.platform("linux_aarch64"),
            _flag.pip_whl("only"),
        ],
        want = "whl",
    )

_tests.append(_test_basic_whl_nosdist)

def _test_whl_default(name):
    _analysis_test(
        name = name,
        dist = {
            "is_cp37_py3_none_any": "whl",
            "is_cp37_py_none_any": "basic_whl",
        },
        want = "whl",
    )

_tests.append(_test_whl_default)

def _test_whl_nowhl(name):
    _analysis_test(
        name = name,
        dist = {
            "is_cp37_py3_none_any": "whl",
            "is_cp37_py_none_any": "basic_whl",
        },
        config_settings = [
            _flag.platform("linux_aarch64"),
            _flag.pip_whl("no"),
        ],
        want = "no_match",
    )

_tests.append(_test_whl_nowhl)

def _test_whl_nosdist(name):
    _analysis_test(
        name = name,
        dist = {
            "is_cp37_py3_none_any": "whl",
        },
        config_settings = [
            _flag.platform("linux_aarch64"),
            _flag.pip_whl("only"),
        ],
        want = "whl",
    )

_tests.append(_test_whl_nosdist)

def _test_abi_whl_is_prefered(name):
    _analysis_test(
        name = name,
        dist = {
            "is_cp37_py3_abi3_any": "abi_whl",
            "is_cp37_py3_none_any": "whl",
        },
        want = "abi_whl",
    )

_tests.append(_test_abi_whl_is_prefered)

def _test_whl_with_constraints_is_prefered(name):
    _analysis_test(
        name = name,
        dist = {
            "is_cp37_py3_none_any": "default_whl",
            "is_cp37_py3_none_any_linux_aarch64": "whl",
            "is_cp37_py3_none_any_linux_x86_64": "amd64_whl",
        },
        want = "whl",
    )

_tests.append(_test_whl_with_constraints_is_prefered)

def _test_cp_whl_is_prefered_over_py3(name):
    _analysis_test(
        name = name,
        dist = {
            "is_cp37_none_any": "cp",
            "is_cp37_py3_abi3_any": "py3_abi3",
            "is_cp37_py3_none_any": "py3",
        },
        want = "cp",
    )

_tests.append(_test_cp_whl_is_prefered_over_py3)

def _test_cp_abi_whl_is_prefered_over_py3(name):
    _analysis_test(
        name = name,
        dist = {
            "is_cp37_abi3_any": "cp",
            "is_cp37_py3_abi3_any": "py3",
        },
        want = "cp",
    )

_tests.append(_test_cp_abi_whl_is_prefered_over_py3)

def _test_cp_version_is_selected_when_python_version_is_specified(name):
    _analysis_test(
        name = name,
        dist = {
            "is_cp310_none_any": "cp310",
            "is_cp38_none_any": "cp38",
            "is_cp39_none_any": "cp39",
        },
        want = "cp310",
        config_settings = [
            _flag.python_version("3.10.9"),
            _flag.platform("linux_aarch64"),
        ],
    )

_tests.append(_test_cp_version_is_selected_when_python_version_is_specified)

def _test_py_none_any_versioned(name):
    _analysis_test(
        name = name,
        dist = {
            "is_cp310_py_none_any": "whl",
            "is_cp39_py_none_any": "too-low",
        },
        want = "whl",
        config_settings = [
            _flag.python_version("3.10.9"),
            _flag.platform("linux_aarch64"),
        ],
    )

_tests.append(_test_py_none_any_versioned)

def _test_cp_whl_is_not_prefered_over_py3_non_freethreaded(name):
    _analysis_test(
        name = name,
        dist = {
            "is_cp37_abi3_any": "py3_abi3",
            "is_cp37_cp37t_any": "cp",
            "is_cp37_none_any": "py3",
        },
        want = "py3_abi3",
        config_settings = [
            _flag.py_freethreaded("no"),
        ],
    )

_tests.append(_test_cp_whl_is_not_prefered_over_py3_non_freethreaded)

def _test_cp_whl_is_not_prefered_over_py3_freethreaded(name):
    _analysis_test(
        name = name,
        dist = {
            "is_cp37_abi3_any": "py3_abi3",
            "is_cp37_cp37_any": "cp",
            "is_cp37_none_any": "py3",
        },
        want = "py3",
        config_settings = [
            _flag.py_freethreaded("yes"),
        ],
    )

_tests.append(_test_cp_whl_is_not_prefered_over_py3_freethreaded)

def _test_cp_cp_whl(name):
    _analysis_test(
        name = name,
        dist = {
            "is_cp310_cp310_linux_aarch64": "whl",
        },
        want = "whl",
        config_settings = [
            _flag.python_version("3.10.9"),
            _flag.platform("linux_aarch64"),
        ],
    )

_tests.append(_test_cp_cp_whl)

def _test_cp_version_sdist_is_selected(name):
    _analysis_test(
        name = name,
        dist = {
            "is_cp310_sdist": "sdist",
        },
        want = "sdist",
        config_settings = [
            _flag.python_version("3.10.9"),
            _flag.platform("linux_aarch64"),
        ],
    )

_tests.append(_test_cp_version_sdist_is_selected)

# NOTE: Right now there is no way to get the following behaviour without
# breaking other tests. We need to choose either ta have the correct
# specialization behaviour between `is_cp37_cp37_any` and
# `is_cp37_cp37_any_linux_aarch64` or this commented out test case.
#
# I think having this behaviour not working is fine because the `suffix`
# will be either present on all of config settings of the same platform
# or none, because we use it as a way to select a separate version of the
# wheel for a single platform only.
#
# If we can think of a better way to handle it, then we can lift this
# limitation.
#
# def _test_any_whl_with_suffix_specialization(name):
#     _analysis_test(
#         name = name,
#         dist = {
#             "is_cp37_abi3_any_linux_aarch64": "abi3",
#             "is_cp37_cp37_any": "cp37",
#         },
#         want = "cp37",
#     )
#
# _tests.append(_test_any_whl_with_suffix_specialization)

def _test_platform_vs_any_with_suffix_specialization(name):
    _analysis_test(
        name = name,
        dist = {
            "is_cp37_cp37_any_linux_aarch64": "any",
            "is_cp37_py3_none_linux_aarch64": "platform_whl",
        },
        want = "platform_whl",
    )

_tests.append(_test_platform_vs_any_with_suffix_specialization)

def _test_platform_whl_is_prefered_over_any_whl_with_constraints(name):
    _analysis_test(
        name = name,
        dist = {
            "is_cp37_py3_abi3_any": "better_default_whl",
            "is_cp37_py3_abi3_any_linux_aarch64": "better_default_any_whl",
            "is_cp37_py3_none_any": "default_whl",
            "is_cp37_py3_none_any_linux_aarch64": "whl",
            "is_cp37_py3_none_linux_aarch64": "platform_whl",
        },
        want = "platform_whl",
    )

_tests.append(_test_platform_whl_is_prefered_over_any_whl_with_constraints)

def _test_abi3_platform_whl_preference(name):
    _analysis_test(
        name = name,
        dist = {
            "is_cp37_py3_abi3_linux_aarch64": "abi3_platform",
            "is_cp37_py3_none_linux_aarch64": "platform",
        },
        want = "abi3_platform",
    )

_tests.append(_test_abi3_platform_whl_preference)

def _test_glibc(name):
    _analysis_test(
        name = name,
        dist = {
            "is_cp37_cp37_manylinux_aarch64": "glibc",
            "is_cp37_py3_abi3_linux_aarch64": "abi3_platform",
        },
        want = "glibc",
    )

_tests.append(_test_glibc)

def _test_glibc_versioned(name):
    _analysis_test(
        name = name,
        dist = {
            "is_cp37_cp37_manylinux_2_14_aarch64": "glibc",
            "is_cp37_cp37_manylinux_2_17_aarch64": "glibc",
            "is_cp37_py3_abi3_linux_aarch64": "abi3_platform",
        },
        want = "glibc",
        config_settings = [
            _flag.py_linux_libc("glibc"),
            _flag.pip_whl_glibc_version("2.17"),
            _flag.platform("linux_aarch64"),
        ],
    )

_tests.append(_test_glibc_versioned)

def _test_glibc_compatible_exists(name):
    _analysis_test(
        name = name,
        dist = {
            # Code using the conditions will need to construct selects, which
            # do the version matching correctly.
            "is_cp37_cp37_manylinux_2_14_aarch64": "2_14_whl_via_2_14_branch",
            "is_cp37_cp37_manylinux_2_17_aarch64": "2_14_whl_via_2_17_branch",
        },
        want = "2_14_whl_via_2_17_branch",
        config_settings = [
            _flag.py_linux_libc("glibc"),
            _flag.pip_whl_glibc_version("2.17"),
            _flag.platform("linux_aarch64"),
        ],
    )

_tests.append(_test_glibc_compatible_exists)

def _test_musl(name):
    _analysis_test(
        name = name,
        dist = {
            "is_cp37_cp37_musllinux_aarch64": "musl",
        },
        want = "musl",
        config_settings = [
            _flag.py_linux_libc("musl"),
            _flag.platform("linux_aarch64"),
        ],
    )

_tests.append(_test_musl)

def _test_windows(name):
    _analysis_test(
        name = name,
        dist = {
            "is_cp37_cp37_windows_x86_64": "whl",
            "is_cp37_cp37t_windows_x86_64": "whl_freethreaded",
        },
        want = "whl",
        config_settings = [
            _flag.platform("windows_x86_64"),
        ],
    )

_tests.append(_test_windows)

def _test_windows_freethreaded(name):
    _analysis_test(
        name = name,
        dist = {
            "is_cp37_cp37_windows_x86_64": "whl",
            "is_cp37_cp37t_windows_x86_64": "whl_freethreaded",
        },
        want = "whl_freethreaded",
        config_settings = [
            _flag.platform("windows_x86_64"),
            _flag.py_freethreaded("yes"),
        ],
    )

_tests.append(_test_windows_freethreaded)

def _test_osx(name):
    _analysis_test(
        name = name,
        dist = {
            # We prefer arch specific whls over universal
            "is_cp37_cp37_osx_universal2": "universal_whl",
            "is_cp37_cp37_osx_x86_64": "whl",
        },
        want = "whl",
        config_settings = [
            _flag.platform("mac_x86_64"),
        ],
    )

_tests.append(_test_osx)

def _test_osx_universal_default(name):
    _analysis_test(
        name = name,
        dist = {
            # We default to universal if only that exists
            "is_cp37_cp37_osx_universal2": "whl",
        },
        want = "whl",
        config_settings = [
            _flag.platform("mac_x86_64"),
        ],
    )

_tests.append(_test_osx_universal_default)

def _test_osx_universal_only(name):
    _analysis_test(
        name = name,
        dist = {
            # If we prefer universal, then we use that
            "is_cp37_cp37_osx_universal2": "universal",
            "is_cp37_cp37_osx_x86_64": "whl",
        },
        want = "universal",
        config_settings = [
            _flag.pip_whl_osx_arch("universal"),
            _flag.platform("mac_x86_64"),
        ],
    )

_tests.append(_test_osx_universal_only)

def _test_osx_os_version(name):
    _analysis_test(
        name = name,
        dist = {
            # Similarly to the libc version, the user of the config settings will have to
            # construct the select so that the version selection is correct.
            "is_cp37_cp37_osx_10_9_x86_64": "whl",
        },
        want = "whl",
        config_settings = [
            _flag.pip_whl_osx_version("10.9"),
            _flag.platform("mac_x86_64"),
        ],
    )

_tests.append(_test_osx_os_version)

def _test_all(name):
    _analysis_test(
        name = name,
        dist = {
            "is_cp37_" + f: f
            for f in [
                "{py}{abi}_{plat}".format(py = valid_py, abi = valid_abi, plat = valid_plat)
                # we have py2.py3, py3, cp3
                for valid_py in ["py_", "py3_", ""]
                # cp abi usually comes with a version and we only need one
                # config setting variant for all of them because the python
                # version will discriminate between different versions.
                for valid_abi in ["none", "abi3", "cp37"]
                for valid_plat in [
                    "any",
                    "manylinux_2_17_x86_64",
                    "manylinux_2_17_aarch64",
                    "osx_x86_64",
                    "windows_x86_64",
                ]
                if not (
                    valid_abi == "abi3" and valid_py == "py_" or
                    valid_abi == "cp37" and valid_py != ""
                )
            ]
        },
        want = "cp37_manylinux_2_17_x86_64",
        config_settings = [
            _flag.pip_whl_glibc_version("2.17"),
            _flag.platform("linux_x86_64"),
        ],
    )

_tests.append(_test_all)

def config_settings_test_suite(name):  # buildifier: disable=function-docstring
    test_suite(
        name = name,
        tests = _tests,
    )

    config_settings(
        name = "dummy",
        python_versions = ["3.7", "3.8", "3.9", "3.10"],
        glibc_versions = [(2, 14), (2, 17)],
        muslc_versions = [(1, 1)],
        osx_versions = [(10, 9), (11, 0)],
        target_platforms = [
            "windows_x86_64",
            "windows_aarch64",
            "linux_x86_64",
            "linux_ppc",
            "linux_aarch64",
            "osx_x86_64",
            "osx_aarch64",
        ],
    )
