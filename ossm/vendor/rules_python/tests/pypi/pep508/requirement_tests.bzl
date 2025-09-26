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
"""Tests for parsing the requirement specifier."""

load("@rules_testing//lib:test_suite.bzl", "test_suite")
load("//python/private/pypi:pep508_requirement.bzl", "requirement")  # buildifier: disable=bzl-visibility

_tests = []

def _test_requirement_line_parsing(env):
    want = {
        " name1[ foo ] ": ("name1", ["foo"], None, ""),
        "Name[foo]": ("name", ["foo"], None, ""),
        "name [fred,bar] @ http://foo.com ; python_version=='2.7'": ("name", ["fred", "bar"], None, "python_version=='2.7'"),
        "name; (os_name=='a' or os_name=='b') and os_name=='c'": ("name", [""], None, "(os_name=='a' or os_name=='b') and os_name=='c'"),
        "name@http://foo.com": ("name", [""], None, ""),
        "name[ Foo123 ]": ("name", ["Foo123"], None, ""),
        "name[extra]@http://foo.com": ("name", ["extra"], None, ""),
        "name[foo]": ("name", ["foo"], None, ""),
        "name[quux, strange];python_version<'2.7' and platform_version=='2'": ("name", ["quux", "strange"], None, "python_version<'2.7' and platform_version=='2'"),
        "name_foo[bar]": ("name-foo", ["bar"], None, ""),
        "name_foo[bar]==0.25": ("name-foo", ["bar"], "0.25", ""),
    }

    got = {
        i: (parsed.name, parsed.extras, parsed.version, parsed.marker)
        for i, parsed in {case: requirement(case) for case in want}.items()
    }
    env.expect.that_dict(got).contains_exactly(want)

_tests.append(_test_requirement_line_parsing)

def requirement_test_suite(name):  # buildifier: disable=function-docstring
    test_suite(
        name = name,
        basic_tests = _tests,
    )
