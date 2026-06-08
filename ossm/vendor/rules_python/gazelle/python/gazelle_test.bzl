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

load("@io_bazel_rules_go//go:def.bzl", "go_test")

def gazelle_test(*, name, test_dirs, **kwargs):
    """A simple macro to better cache gazelle integration tests

    Args:
        name (str): The name of the test suite target to be created and
            the prefix to all of the individual test targets.
        test_dirs (list[str]): The list of dirs in the 'testdata'
            directory that we should create separate 'go_test' cases for.
            Each of them will be prefixed with '{name}'.
        **kwargs: extra arguments passed to 'go_test'.
    """
    tests = []

    data = kwargs.pop("data", [])

    for dir in test_dirs:
        _, _, basename = dir.rpartition("/")

        test = "{}_{}".format(name, basename)
        tests.append(test)

        go_test(
            name = test,
            data = native.glob(["{}/**".format(dir)]) + data,
            **kwargs
        )

    native.test_suite(
        name = name,
        tests = tests,
    )
