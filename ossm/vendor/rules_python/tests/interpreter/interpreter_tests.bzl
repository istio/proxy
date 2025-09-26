# Copyright 2025 The Bazel Authors. All rights reserved.
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

"""This file contains helpers for testing the interpreter rule."""

load("//tests/support:py_reconfig.bzl", "py_reconfig_test")

# The versions of Python that we want to run the interpreter tests against.
PYTHON_VERSIONS_TO_TEST = (
    "3.10",
    "3.11",
    "3.12",
)

def py_reconfig_interpreter_tests(name, python_versions, env = {}, **kwargs):
    """Runs the specified test against each of the specified Python versions.

    One test gets generated for each Python version. The following environment
    variable gets set for the test:

        EXPECTED_SELF_VERSION: Contains the Python version that the test itself
            is running under.

    Args:
        name: Name of the test.
        python_versions: A list of Python versions to test.
        env: The environment to set on the test.
        **kwargs: Passed to the underlying py_reconfig_test targets.
    """
    for python_version in python_versions:
        py_reconfig_test(
            name = "{}_{}".format(name, python_version),
            env = env | {
                "EXPECTED_SELF_VERSION": python_version,
            },
            python_version = python_version,
            **kwargs
        )

    native.test_suite(
        name = name,
        tests = [":{}_{}".format(name, python_version) for python_version in python_versions],
    )
