# Copyright 2023 The Bazel Authors. All rights reserved.
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
load("//python/private/pypi:whl_target_platforms.bzl", "whl_target_platforms")  # buildifier: disable=bzl-visibility

_tests = []

def _test_simple(env):
    tests = {
        "macosx_10_9_arm64": [
            struct(os = "osx", cpu = "aarch64", abi = None, target_platform = "osx_aarch64", version = (10, 9)),
        ],
        "macosx_10_9_universal2": [
            struct(os = "osx", cpu = "x86_64", abi = None, target_platform = "osx_x86_64", version = (10, 9)),
            struct(os = "osx", cpu = "aarch64", abi = None, target_platform = "osx_aarch64", version = (10, 9)),
        ],
        "manylinux_2_17_i686": [
            struct(os = "linux", cpu = "x86_32", abi = None, target_platform = "linux_x86_32", version = (2, 17)),
        ],
        "musllinux_1_1_ppc64le": [
            struct(os = "linux", cpu = "ppc64le", abi = None, target_platform = "linux_ppc64le", version = (1, 1)),
        ],
        "musllinux_1_2_riscv64": [
            struct(os = "linux", cpu = "riscv64", abi = None, target_platform = "linux_riscv64", version = (1, 2)),
        ],
        "win_amd64": [
            struct(os = "windows", cpu = "x86_64", abi = None, target_platform = "windows_x86_64", version = (0, 0)),
        ],
    }

    for give, want in tests.items():
        for abi in ["", "abi3", "none"]:
            got = whl_target_platforms(give, abi)
            env.expect.that_collection(got).contains_exactly(want)

_tests.append(_test_simple)

def _test_with_abi(env):
    tests = {
        "macosx_10_9_arm64": [
            struct(os = "osx", cpu = "aarch64", abi = "cp39", target_platform = "cp39_osx_aarch64", version = (10, 9)),
        ],
        "macosx_10_9_universal2": [
            struct(os = "osx", cpu = "x86_64", abi = "cp310", target_platform = "cp310_osx_x86_64", version = (10, 9)),
            struct(os = "osx", cpu = "aarch64", abi = "cp310", target_platform = "cp310_osx_aarch64", version = (10, 9)),
        ],
        # This should use version 0 because there are two platform_tags. This is
        # just to ensure that the code is robust
        "manylinux1_i686.manylinux_2_17_i686": [
            struct(os = "linux", cpu = "x86_32", abi = "cp38", target_platform = "cp38_linux_x86_32", version = (0, 0)),
        ],
        "musllinux_1_1_ppc64": [
            struct(os = "linux", cpu = "ppc", abi = "cp311", target_platform = "cp311_linux_ppc", version = (1, 1)),
        ],
        "musllinux_1_1_ppc64le": [
            struct(os = "linux", cpu = "ppc64le", abi = "cp311", target_platform = "cp311_linux_ppc64le", version = (1, 1)),
        ],
        "musllinux_1_2_riscv64": [
            struct(os = "linux", cpu = "riscv64", abi = "cp311", target_platform = "cp311_linux_riscv64", version = (1, 2)),
        ],
        "win_amd64": [
            struct(os = "windows", cpu = "x86_64", abi = "cp311", target_platform = "cp311_windows_x86_64", version = (0, 0)),
        ],
    }

    for give, want in tests.items():
        got = whl_target_platforms(give, want[0].abi)
        env.expect.that_collection(got).contains_exactly(want)

_tests.append(_test_with_abi)

def _can_parse_existing_tags(env):
    examples = {
        "linux_armv6l": 1,
        "linux_armv7l": 1,
        "macosx_11_12_arm64": 1,
        "macosx_11_12_i386": 1,
        "macosx_11_12_intel": 1,
        "macosx_11_12_universal": 2,
        "macosx_11_12_universal2": 2,
        "macosx_11_12_x86_64": 1,
        "manylinux1_i686": 1,
        "manylinux1_x86_64": 1,
        "manylinux2010_i686": 1,
        "manylinux2010_x86_64": 1,
        "manylinux2014_aarch64": 1,
        "manylinux2014_armv7l": 1,
        "manylinux2014_i686": 1,
        "manylinux2014_ppc64": 1,
        "manylinux2014_ppc64le": 1,
        "manylinux2014_s390x": 1,
        "manylinux2014_x86_64": 1,
        "manylinux_11_12_aarch64": 1,
        "manylinux_11_12_armv7l": 1,
        "manylinux_11_12_i686": 1,
        "manylinux_11_12_ppc64": 1,
        "manylinux_11_12_ppc64le": 1,
        "manylinux_11_12_riscv64": 1,
        "manylinux_11_12_s390x": 1,
        "manylinux_11_12_x86_64": 1,
        "manylinux_1_2_aarch64": 1,
        "manylinux_1_2_x86_64": 1,
        "musllinux_11_12_aarch64": 1,
        "musllinux_11_12_armv7l": 1,
        "musllinux_11_12_i686": 1,
        "musllinux_11_12_ppc64le": 1,
        "musllinux_11_12_riscv64": 1,
        "musllinux_11_12_s390x": 1,
        "musllinux_11_12_x86_64": 1,
        "win32": 1,
        "win_amd64": 1,
        "win_arm64": 1,
        "win_ia64": 0,
    }

    for major_version in [2, 10, 13]:
        for minor_version in [0, 1, 2, 10, 45]:
            for give, want_size in examples.items():
                give = give.replace("_11_", "_{}_".format(major_version))
                give = give.replace("_12_", "_{}_".format(minor_version))
                got = whl_target_platforms(give)
                env.expect.that_str("{}: {}".format(give, len(got))).equals("{}: {}".format(give, want_size))

_tests.append(_can_parse_existing_tags)

def whl_target_platforms_test_suite(name):
    """create the test suite.

    args:
        name: the name of the test suite
    """
    test_suite(name = name, basic_tests = _tests)
