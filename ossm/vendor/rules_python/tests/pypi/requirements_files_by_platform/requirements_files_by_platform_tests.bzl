# Copyright 2024 The Bazel Authors. All rights reserved.
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
load("//python/private/pypi:requirements_files_by_platform.bzl", _sut = "requirements_files_by_platform")  # buildifier: disable=bzl-visibility

_tests = []

requirements_files_by_platform = lambda **kwargs: _sut(
    platforms = kwargs.pop(
        "platforms",
        [
            "linux_aarch64",
            "linux_arm",
            "linux_ppc",
            "linux_s390x",
            "linux_x86_64",
            "osx_aarch64",
            "osx_x86_64",
            "windows_x86_64",
        ],
    ),
    **kwargs
)

def _test_fail_no_requirements(env):
    errors = []
    requirements_files_by_platform(
        fail_fn = errors.append,
    )
    env.expect.that_str(errors[0]).equals("""\
A 'requirements_lock' attribute must be specified, a platform-specific lockfiles via 'requirements_by_platform' or an os-specific lockfiles must be specified via 'requirements_*' attributes""")

_tests.append(_test_fail_no_requirements)

def _test_fail_duplicate_platforms(env):
    errors = []
    requirements_files_by_platform(
        requirements_by_platform = {
            "requirements_linux": "linux_x86_64",
            "requirements_lock": "*",
        },
        fail_fn = errors.append,
    )
    env.expect.that_collection(errors).has_size(1)
    env.expect.that_str(",".join(errors)).equals("Expected the platform 'linux_x86_64' to be map only to a single requirements file, but got multiple: 'requirements_linux', 'requirements_lock'")

_tests.append(_test_fail_duplicate_platforms)

def _test_fail_download_only_bad_attr(env):
    errors = []
    requirements_files_by_platform(
        requirements_linux = "requirements_linux",
        requirements_osx = "requirements_osx",
        extra_pip_args = [
            "--platform",
            "manylinux_2_27_x86_64",
            "--platform=manylinux_2_12_x86_64",
            "--platform manylinux_2_5_x86_64",
        ],
        fail_fn = errors.append,
    )
    env.expect.that_str(errors[0]).equals("only a single 'requirements_lock' file can be used when using '--platform' pip argument, consider specifying it via 'requirements_lock' attribute")

_tests.append(_test_fail_download_only_bad_attr)

def _test_simple(env):
    for got in [
        requirements_files_by_platform(
            requirements_lock = "requirements_lock",
        ),
        requirements_files_by_platform(
            requirements_by_platform = {
                "requirements_lock": "*",
            },
        ),
    ]:
        env.expect.that_dict(got).contains_exactly({
            "requirements_lock": [
                "linux_aarch64",
                "linux_arm",
                "linux_ppc",
                "linux_s390x",
                "linux_x86_64",
                "osx_aarch64",
                "osx_x86_64",
                "windows_x86_64",
            ],
        })

_tests.append(_test_simple)

def _test_simple_limited(env):
    for got in [
        requirements_files_by_platform(
            requirements_lock = "requirements_lock",
            platforms = ["linux_x86_64", "osx_x86_64"],
        ),
        requirements_files_by_platform(
            requirements_by_platform = {
                "requirements_lock": "*",
            },
            platforms = ["linux_x86_64", "osx_x86_64"],
        ),
        requirements_files_by_platform(
            requirements_by_platform = {
                "requirements_lock": "linux_x86_64,osx_aarch64,osx_x86_64",
            },
            platforms = ["linux_x86_64", "osx_x86_64", "windows_x86_64"],
        ),
    ]:
        env.expect.that_dict(got).contains_exactly({
            "requirements_lock": [
                "linux_x86_64",
                "osx_x86_64",
            ],
        })

_tests.append(_test_simple_limited)

def _test_simple_with_python_version(env):
    for got in [
        requirements_files_by_platform(
            requirements_lock = "requirements_lock",
            python_version = "3.11",
        ),
        requirements_files_by_platform(
            requirements_by_platform = {
                "requirements_lock": "*",
            },
            python_version = "3.11",
        ),
        # TODO @aignas 2024-07-15: consider supporting this way of specifying
        # the requirements without the need of the `python_version` attribute
        # setting. However, this might need more tweaks, hence only leaving a
        # comment in the test.
        # requirements_files_by_platform(
        #     requirements_by_platform = {
        #         "requirements_lock": "cp311_*",
        #     },
        # ),
    ]:
        env.expect.that_dict(got).contains_exactly({
            "requirements_lock": [
                "cp311_linux_aarch64",
                "cp311_linux_arm",
                "cp311_linux_ppc",
                "cp311_linux_s390x",
                "cp311_linux_x86_64",
                "cp311_osx_aarch64",
                "cp311_osx_x86_64",
                "cp311_windows_x86_64",
            ],
        })

_tests.append(_test_simple_with_python_version)

def _test_multi_os(env):
    for got in [
        requirements_files_by_platform(
            requirements_linux = "requirements_linux",
            requirements_osx = "requirements_osx",
            requirements_windows = "requirements_windows",
        ),
        requirements_files_by_platform(
            requirements_by_platform = {
                "requirements_linux": "linux_*",
                "requirements_osx": "osx_*",
                "requirements_windows": "windows_*",
            },
        ),
    ]:
        env.expect.that_dict(got).contains_exactly({
            "requirements_linux": [
                "linux_aarch64",
                "linux_arm",
                "linux_ppc",
                "linux_s390x",
                "linux_x86_64",
            ],
            "requirements_osx": [
                "osx_aarch64",
                "osx_x86_64",
            ],
            "requirements_windows": [
                "windows_x86_64",
            ],
        })

_tests.append(_test_multi_os)

def _test_multi_os_download_only_platform(env):
    got = requirements_files_by_platform(
        requirements_lock = "requirements_linux",
        extra_pip_args = [
            "--platform",
            "manylinux_2_27_x86_64",
            "--platform=manylinux_2_12_x86_64",
            "--platform manylinux_2_5_x86_64",
        ],
    )
    env.expect.that_dict(got).contains_exactly({
        "requirements_linux": ["linux_x86_64"],
    })

_tests.append(_test_multi_os_download_only_platform)

def _test_os_arch_requirements_with_default(env):
    got = requirements_files_by_platform(
        requirements_by_platform = {
            "requirements_exotic": "linux_super_exotic",
            "requirements_linux": "linux_x86_64,linux_aarch64",
        },
        requirements_lock = "requirements_lock",
        platforms = [
            "linux_super_exotic",
            "linux_x86_64",
            "linux_aarch64",
            "linux_arm",
            "linux_ppc",
            "linux_s390x",
            "osx_aarch64",
            "osx_x86_64",
            "windows_x86_64",
        ],
    )
    env.expect.that_dict(got).contains_exactly({
        "requirements_exotic": ["linux_super_exotic"],
        "requirements_linux": ["linux_x86_64", "linux_aarch64"],
        "requirements_lock": [
            "linux_arm",
            "linux_ppc",
            "linux_s390x",
            "osx_aarch64",
            "osx_x86_64",
            "windows_x86_64",
        ],
    })

_tests.append(_test_os_arch_requirements_with_default)

def requirements_files_by_platform_test_suite(name):
    """Create the test suite.

    Args:
        name: the name of the test suite
    """
    test_suite(name = name, basic_tests = _tests)
