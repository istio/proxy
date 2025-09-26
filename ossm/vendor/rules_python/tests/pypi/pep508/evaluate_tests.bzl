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

load("@rules_testing//lib:test_suite.bzl", "test_suite")
load("//python/private/pypi:pep508_env.bzl", pep508_env = "env")  # buildifier: disable=bzl-visibility
load("//python/private/pypi:pep508_evaluate.bzl", "evaluate", "tokenize")  # buildifier: disable=bzl-visibility

_tests = []

def _check_evaluate(env, expr, expected, values, strict = True):
    env.expect.where(
        expression = expr,
        values = values,
    ).that_bool(evaluate(expr, env = values, strict = strict)).equals(expected)

def _tokenize_tests(env):
    for input, want in {
        "": [],
        "'osx' == os_name": ['"osx"', "==", "os_name"],
        "'x' not in os_name": ['"x"', "not in", "os_name"],
        "()": ["(", ")"],
        "(os_name == 'osx' and not os_name == 'posix') or os_name == \"win\"": [
            "(",
            "os_name",
            "==",
            '"osx"',
            "and",
            "not",
            "os_name",
            "==",
            '"posix"',
            ")",
            "or",
            "os_name",
            "==",
            '"win"',
        ],
        "os_name\t==\t'osx'": ["os_name", "==", '"osx"'],
        "os_name == 'osx'": ["os_name", "==", '"osx"'],
        "python_version <= \"1.0\"": ["python_version", "<=", '"1.0"'],
        "python_version>='1.0.0'": ["python_version", ">=", '"1.0.0"'],
        "python_version~='1.0.0'": ["python_version", "~=", '"1.0.0"'],
    }.items():
        got = tokenize(input)
        env.expect.that_collection(got).contains_exactly(want).in_order()

_tests.append(_tokenize_tests)

def _evaluate_non_version_env_tests(env):
    for var_name in [
        "implementation_name",
        "os_name",
        "platform_machine",
        "platform_python_implementation",
        "platform_release",
        "platform_system",
        "sys_platform",
        "extra",
    ]:
        # Given
        marker_env = {var_name: "osx"}

        # When
        for input, want in {
            "'osx' != {}".format(var_name): False,
            "'osx' < {}".format(var_name): False,
            "'osx' <= {}".format(var_name): True,
            "'osx' == {}".format(var_name): True,
            "'osx' >= {}".format(var_name): True,
            "'w' not in {}".format(var_name): True,
            "'x' in {}".format(var_name): True,
            "{} != 'osx'".format(var_name): False,
            "{} < 'osx'".format(var_name): False,
            "{} <= 'osx'".format(var_name): True,
            "{} == 'osx'".format(var_name): True,
            "{} > 'osx'".format(var_name): False,
            "{} >= 'osx'".format(var_name): True,
        }.items():
            _check_evaluate(env, input, want, marker_env)

            # Check that the non-strict eval gives us back the input when no
            # env is supplied.
            _check_evaluate(env, input, input.replace("'", '"'), {}, strict = False)

_tests.append(_evaluate_non_version_env_tests)

def _evaluate_version_env_tests(env):
    for var_name in [
        "python_version",
        "implementation_version",
        "platform_version",
        "python_full_version",
    ]:
        # Given
        marker_env = {var_name: "3.7.9"}

        # When
        for input, want in {
            "{} < '3.8'".format(var_name): True,
            "{} > '3.7'".format(var_name): True,
            "{} >= '3.7.9'".format(var_name): True,
            "{} >= '3.7.10'".format(var_name): False,
            "{} >= '3.7.8'".format(var_name): True,
            "{} <= '3.7.9'".format(var_name): True,
            "{} <= '3.7.10'".format(var_name): True,
            "{} <= '3.7.8'".format(var_name): False,
            "{} == '3.7.9'".format(var_name): True,
            "{} == '3.7.*'".format(var_name): True,
            "{} != '3.7.9'".format(var_name): False,
            "{} ~= '3.7.1'".format(var_name): True,
            "{} ~= '3.7.10'".format(var_name): False,
            "{} ~= '3.8.0'".format(var_name): False,
            "{} === '3.7.9+rc2'".format(var_name): False,
            "{} === '3.7.9'".format(var_name): True,
            "{} == '3.7.9+rc2'".format(var_name): True,
        }.items():  # buildifier: @unsorted-dict-items
            _check_evaluate(env, input, want, marker_env)

            # Check that the non-strict eval gives us back the input when no
            # env is supplied.
            _check_evaluate(env, input, input.replace("'", '"'), {}, strict = False)

_tests.append(_evaluate_version_env_tests)

def _evaluate_platform_version_is_special(env):
    # Given
    marker_env = {"platform_version": "FooBar Linux v1.2.3"}

    # When the platform version is not
    input = "platform_version == '0'"
    _check_evaluate(env, input, False, marker_env)

    # And when I compare it as string
    input = "'FooBar' in platform_version"
    _check_evaluate(env, input, True, marker_env)

    # Check that the non-strict eval gives us back the input when no
    # env is supplied.
    _check_evaluate(env, input, input.replace("'", '"'), {}, strict = False)

_tests.append(_evaluate_platform_version_is_special)

def _logical_expression_tests(env):
    for input, want in {
        # Basic
        "": True,
        "(())": True,
        "()": True,

        # expr
        "os_name == 'fo'": False,
        "(os_name == 'fo')": False,
        "((os_name == 'fo'))": False,
        "((os_name == 'foo'))": True,
        "not (os_name == 'fo')": True,

        # and
        "os_name == 'fo' and os_name == 'foo'": False,

        # and not
        "os_name == 'fo' and not os_name == 'foo'": False,

        # or
        "os_name == 'oo' or os_name == 'foo'": True,

        # or not
        "os_name == 'foo' or not os_name == 'foo'": True,

        # multiple or
        "os_name == 'oo' or os_name == 'fo' or os_name == 'foo'": True,
        "os_name == 'oo' or os_name == 'foo' or os_name == 'fo'": True,

        # multiple and
        "os_name == 'foo' and os_name == 'foo' and os_name == 'fo'": False,

        # x or not y and z != (x or not y), but is instead evaluated as x or (not y and z)
        "os_name == 'foo' or not os_name == 'fo' and os_name == 'fo'": True,

        # x or y and z != (x or y) and z, but is instead evaluated as x or (y and z)
        "os_name == 'foo' or os_name == 'fo' and os_name == 'fo'": True,
        "not (os_name == 'foo' or os_name == 'fo' and os_name == 'fo')": False,

        # x or y and z and w != (x or y and z) and w, but is instead evaluated as x or (y and z and w)
        "os_name == 'foo' or os_name == 'fo' and os_name == 'fo' and os_name == 'fo'": True,

        # not not True
        "not not os_name == 'foo'": True,
        "not not not os_name == 'foo'": False,
    }.items():  # buildifier: @unsorted-dict-items
        _check_evaluate(env, input, want, {"os_name": "foo"})

        if not input.strip("()"):
            # These cases will just return True, because they will be evaluated
            # and the brackets will be processed.
            continue

        # Check that the non-strict eval gives us back the input when no env
        # is supplied.
        _check_evaluate(env, input, input.replace("'", '"'), {}, strict = False)

_tests.append(_logical_expression_tests)

def _evaluate_partial_only_extra(env):
    # Given
    extra = "foo"

    # When
    for input, want in {
        "os_name == 'osx' and extra == 'bar'": False,
        "os_name == 'osx' and extra == 'foo'": "os_name == \"osx\"",
        "platform_system == 'aarch64' and os_name == 'osx' and extra == 'foo'": "platform_system == \"aarch64\" and os_name == \"osx\"",
        "platform_system == 'aarch64' and extra == 'foo' and os_name == 'osx'": "platform_system == \"aarch64\" and os_name == \"osx\"",
        "os_name == 'osx' or extra == 'bar'": "os_name == \"osx\"",
        "os_name == 'osx' or extra == 'foo'": "",
        "extra == 'bar' or os_name == 'osx'": "os_name == \"osx\"",
        "extra == 'foo' or os_name == 'osx'": "",
        "os_name == 'win' or extra == 'bar' or os_name == 'osx'": "os_name == \"win\" or os_name == \"osx\"",
        "os_name == 'win' or extra == 'foo' or os_name == 'osx'": "",
    }.items():  # buildifier: @unsorted-dict-items
        got = evaluate(
            input,
            env = {
                "extra": extra,
            },
            strict = False,
        )
        env.expect.that_bool(got).equals(want)
        _check_evaluate(env, input, want, {"extra": extra}, strict = False)

_tests.append(_evaluate_partial_only_extra)

def _evaluate_with_aliases(env):
    # When
    for (os, cpu), tests in {
        # buildifier: @unsorted-dict-items
        ("osx", "aarch64"): {
            "platform_system == 'Darwin' and platform_machine == 'arm64'": True,
            "platform_system == 'Darwin' and platform_machine == 'aarch64'": True,
            "platform_system == 'Darwin' and platform_machine == 'amd64'": False,
        },
        ("osx", "x86_64"): {
            "platform_system == 'Darwin' and platform_machine == 'amd64'": True,
            "platform_system == 'Darwin' and platform_machine == 'x86_64'": True,
        },
        ("osx", "x86_32"): {
            "platform_system == 'Darwin' and platform_machine == 'i386'": True,
            "platform_system == 'Darwin' and platform_machine == 'i686'": True,
            "platform_system == 'Darwin' and platform_machine == 'x86_32'": True,
            "platform_system == 'Darwin' and platform_machine == 'x86_64'": False,
        },
        ("freebsd", "x86_32"): {
            "platform_system == 'FreeBSD' and platform_machine == 'i386'": True,
            "platform_system == 'FreeBSD' and platform_machine == 'i686'": True,
            "platform_system == 'FreeBSD' and platform_machine == 'x86_32'": True,
            "platform_system == 'FreeBSD' and platform_machine == 'x86_64'": False,
            "platform_system == 'FreeBSD' and os_name == 'posix'": True,
        },
    }.items():  # buildifier: @unsorted-dict-items
        for input, want in tests.items():
            _check_evaluate(env, input, want, pep508_env(
                os = os,
                arch = cpu,
                python_version = "3.2",
            ))

_tests.append(_evaluate_with_aliases)

def _expr_case(expr, want, env):
    return struct(expr = expr.strip(), want = want, env = env)

_MISC_EXPRESSIONS = [
    _expr_case('python_version == "3.*"', True, {"python_version": "3.10.1"}),
    _expr_case('python_version != "3.10.*"', False, {"python_version": "3.10.1"}),
    _expr_case('python_version != "3.11.*"', True, {"python_version": "3.10.1"}),
    _expr_case('python_version != "3.10"', False, {"python_version": "3.10.0"}),
    _expr_case('python_version == "3.10"', True, {"python_version": "3.10.0"}),
    # Cases for the '>' operator
    # Taken from spec: https://peps.python.org/pep-0440/#exclusive-ordered-comparison
    _expr_case('python_version > "1.7"', True, {"python_version": "1.7.1"}),
    _expr_case('python_version > "1.7"', False, {"python_version": "1.7.0.post0"}),
    _expr_case('python_version > "1.7"', True, {"python_version": "1.7.1"}),
    _expr_case('python_version > "1.7.post2"', True, {"python_version": "1.7.1"}),
    _expr_case('python_version > "1.7.post2"', True, {"python_version": "1.7.post3"}),
    _expr_case('python_version > "1.7.post2"', False, {"python_version": "1.7.0"}),
    _expr_case('python_version > "1.7.1+local"', False, {"python_version": "1.7.1"}),
    _expr_case('python_version > "1.7.1+local"', True, {"python_version": "1.7.2"}),
    # Extra cases for the '<' operator
    _expr_case('python_version < "1.7.1"', False, {"python_version": "1.7.2"}),
    _expr_case('python_version < "1.7.3"', True, {"python_version": "1.7.2"}),
    _expr_case('python_version < "1.7.1"', True, {"python_version": "1.7"}),
    _expr_case('python_version < "1.7.1"', False, {"python_version": "1.7.1-rc2"}),
    _expr_case('python_version < "1.7.1-rc3"', True, {"python_version": "1.7.1-rc2"}),
    _expr_case('python_version < "1.7.1-rc1"', False, {"python_version": "1.7.1-rc2"}),
    # Extra tests
    _expr_case('python_version <= "1.7.1"', True, {"python_version": "1.7.1"}),
    _expr_case('python_version <= "1.7.2"', True, {"python_version": "1.7.1"}),
    _expr_case('python_version >= "1.7.1"', True, {"python_version": "1.7.1"}),
    _expr_case('python_version >= "1.7.0"', True, {"python_version": "1.7.1"}),
    # Compatible version tests:
    # https://packaging.python.org/en/latest/specifications/version-specifiers/#compatible-release
    _expr_case('python_version ~= "2.2"', True, {"python_version": "2.3"}),
    _expr_case('python_version ~= "2.2"', False, {"python_version": "2.1"}),
    _expr_case('python_version ~= "2.2.post3"', False, {"python_version": "2.2"}),
    _expr_case('python_version ~= "2.2.post3"', True, {"python_version": "2.3"}),
    _expr_case('python_version ~= "2.2.post3"', False, {"python_version": "3.0"}),
    _expr_case('python_version ~= "1!2.2"', False, {"python_version": "2.7"}),
    _expr_case('python_version ~= "0!2.2"', True, {"python_version": "2.7"}),
    _expr_case('python_version ~= "1!2.2"', True, {"python_version": "1!2.7"}),
    _expr_case('python_version ~= "1.2.3"', True, {"python_version": "1.2.4"}),
    _expr_case('python_version ~= "1.2.3"', False, {"python_version": "1.3.2"}),
]

def _misc_expressions(env):
    for case in _MISC_EXPRESSIONS:
        _check_evaluate(env, case.expr, case.want, case.env)

_tests.append(_misc_expressions)

def evaluate_test_suite(name):  # buildifier: disable=function-docstring
    test_suite(
        name = name,
        basic_tests = _tests,
    )
