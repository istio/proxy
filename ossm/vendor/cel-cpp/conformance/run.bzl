# Copyright 2024 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""
This module contains build rules for generating the conformance test targets.
"""

load("@rules_cc//cc:cc_test.bzl", "cc_test")

# Converts the list of tests to skip from the format used by the original Go test runner to a single
# flag value where each test is separated by a comma. It also performs expansion, for example
# `foo/bar,baz` becomes two entries which are `foo/bar` and `foo/baz`.
def _expand_tests_to_skip(tests_to_skip):
    result = []
    for test_to_skip in tests_to_skip:
        comma = test_to_skip.find(",")
        if comma == -1:
            result.append(test_to_skip)
            continue
        slash = test_to_skip.rfind("/", 0, comma)
        if slash == -1:
            slash = 0
        else:
            slash = slash + 1
        for part in test_to_skip[slash:].split(","):
            result.append(test_to_skip[0:slash] + part)
    return result

def _conformance_test_name(name, optimize, recursive):
    return "_".join(
        [
            name,
            "optimized" if optimize else "unoptimized",
            "recursive" if recursive else "iterative",
        ],
    )

def _conformance_test_args(modern, optimize, recursive, skip_check, skip_tests, dashboard):
    args = []
    if modern:
        args.append("--modern")
    if optimize:
        args.append("--opt")
    if recursive:
        args.append("--recursive")
    if skip_check:
        args.append("--skip_check")
    else:
        args.append("--noskip_check")
    args.append("--skip_tests={}".format(",".join(_expand_tests_to_skip(skip_tests))))
    if dashboard:
        args.append("--dashboard")
    return args

def _conformance_test(name, data, modern, optimize, recursive, skip_check, skip_tests, tags, dashboard):
    cc_test(
        name = _conformance_test_name(name, optimize, recursive),
        args = _conformance_test_args(modern, optimize, recursive, skip_check, skip_tests, dashboard) + ["$(location " + test + ")" for test in data],
        data = data,
        deps = ["//conformance:run"],
        tags = tags,
    )

def gen_conformance_tests(name, data, modern = False, checked = False, dashboard = False, skip_tests = [], tags = []):
    """Generates conformance tests.

    Args:
        name: prefix for all tests
        modern: run using modern APIs
        checked: whether to apply type checking
        data: textproto targets describing conformance tests
        skip_tests: tests to skip in the format of the cel-spec test runner. See documentation
            in github.com/google/cel-spec/tests/simple/simple_test.go
        tags: tags added to the generated targets
        dashboard: enable dashboard mode
    """
    skip_check = not checked
    tests = []
    for optimize in (True, False):
        for recursive in (True, False):
            test_name = _conformance_test_name(name, optimize, recursive)
            tests.append(test_name)
            _conformance_test(
                name,
                data,
                modern = modern,
                optimize = optimize,
                recursive = recursive,
                skip_check = skip_check,
                skip_tests = skip_tests,
                tags = tags,
                dashboard = dashboard,
            )
    native.test_suite(
        name = name,
        tests = tests,
        tags = tags,
    )
