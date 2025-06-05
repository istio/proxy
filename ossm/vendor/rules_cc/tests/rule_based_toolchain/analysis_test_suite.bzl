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
"""Test suites for the rule based toolchain."""

load("@rules_testing//lib:analysis_test.bzl", "analysis_test")
load(":subjects.bzl", "FACTORIES")

visibility("//tests/rule_based_toolchain/...")

_DEFAULT_TARGET = "//tests/rule_based_toolchain/actions:c_compile"

# Tests of internal starlark functions will often not require any targets,
# but analysis_test requires at least one, so we pick an arbitrary one.
def analysis_test_suite(name, tests, targets = [_DEFAULT_TARGET]):
    """A test suite for the internals of the toolchain.

    Args:
      name: (str) The name of the test suite.
      tests: (dict[str, fn]) A mapping from test name to implementations.
      targets: (List[Label|str]) List of targets accessible to the test.
    """
    targets = [native.package_relative_label(target) for target in targets]

    test_case_names = []
    for test_name, impl in tests.items():
        if not test_name.endswith("_test"):
            fail("Expected test keys to end with '_test', got test case %r" % test_name)
        test_case_names.append(":" + test_name)
        analysis_test(
            name = test_name,
            impl = impl,
            provider_subject_factories = FACTORIES,
            targets = {label.name: label for label in targets},
        )

    native.test_suite(
        name = name,
        tests = test_case_names,
    )
