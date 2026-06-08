# Copyright 2025 The Helly25 Authors.
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

"""Unit tests for versions.bzl."""

load("@bazel_skylib//lib:unittest.bzl", "asserts", "unittest")
load("//bzl/versions:versions.bzl", "versions")

def _assert_eq(env, result, expected, msg = None):
    asserts.equals(env, expected, result, msg)

def _versions_parse_test(ctx):
    """Unit tests for `versions.parse`."""
    env = unittest.begin(ctx)

    _assert_eq(env, versions.parse("25"), [25])
    _assert_eq(env, versions.parse("25.33.42.bla"), [25, 33, 42, "bla"])
    _assert_eq(env, versions.parse(""), [])
    # Parsing '25.33.bla' does not work because 'bla' is not a number.
    # _assert_eq(env, versions.parse("25.33.bla"), [25, 33, 42, "bla"])

    _assert_eq(env, versions.parse(25), [25])
    _assert_eq(env, versions.parse("26"), [26])
    _assert_eq(env, versions.parse([27]), [27])
    _assert_eq(env, versions.parse((28)), [28])

    _assert_eq(env, versions.parse(("25", "33")), [25, 33])
    _assert_eq(env, versions.parse(("25", "33", "42", "bla")), [25, 33, 42, "bla"])

    _assert_eq(env, versions.parse("25.30.42-pre.release+build.fun"), [25, 30, 42, "-", "pre", "release", "+", "build", "fun"])
    _assert_eq(env, versions.parse("25.31.42-pre-release+build+fun"), [25, 31, 42, "-", "pre-release", "+", "build+fun"])
    _assert_eq(env, versions.parse("25.32.42-pre-release+build-fun"), [25, 32, 42, "-", "pre-release", "+", "build-fun"])
    _assert_eq(env, versions.parse("25.33.42+build.fun-foo.bar+baz"), [25, 33, 42, "+", "build", "fun-foo", "bar+baz"])
    _assert_eq(env, versions.parse("25.34.42+build-fun-foo.bar+baz"), [25, 34, 42, "+", "build-fun-foo", "bar+baz"])
    _assert_eq(env, versions.parse("25.35.42+build+fun-foo.bar+baz"), [25, 35, 42, "+", "build+fun-foo", "bar+baz"])

    _assert_eq(env, versions.parse("25.35.42-rc1.beta-11.alpha--111+rc1"), [25, 35, 42, "-", "rc", 1, "beta", 11, "alpha-", 111, "+", "rc1"])

    _assert_eq(env, versions.parse("25.30.42-pre+build"), [25, 30, 42, "-", "pre", "+", "build"])
    _assert_eq(env, versions.parse("25.30.42-pre+build", skip_build = False), [25, 30, 42, "-", "pre", "+", "build"])
    _assert_eq(env, versions.parse("25.30.42-pre+build", skip_build = True), [25, 30, 42, "-", "pre"])
    _assert_eq(env, versions.parse("25.30.42+build"), [25, 30, 42, "+", "build"])
    _assert_eq(env, versions.parse("25.30.42+build", skip_build = False), [25, 30, 42, "+", "build"])
    _assert_eq(env, versions.parse("25.30.42+build", skip_build = True), [25, 30, 42])

    return unittest.end(env)

def _test_cmp(env, lhs, rhs, expected, **kwargs):
    _assert_eq(env, versions.cmp(lhs, rhs, **kwargs), expected, "{lhs} <=> {rhs}".format(
        lhs = lhs,
        rhs = rhs,
    ))

def _test_op(env, lhs, op, rhs, expected, **kwargs):
    _assert_eq(env, versions.compare(lhs, op, rhs, **kwargs), expected, "{lhs} {op} {rhs}".format(
        lhs = lhs,
        op = op,
        rhs = rhs,
    ))

def _test_op_eq_cmp(env, lhs, op, rhs, **kwargs):
    cmp_result = versions.cmp(lhs, rhs, **kwargs)
    if op == ">=":
        expected = cmp_result >= 0
    elif op == "<=":
        expected = cmp_result <= 0
    elif op == ">":
        expected = cmp_result == 1
    elif op == "<":
        expected = cmp_result == -1
    elif op == "==":
        expected = cmp_result == 0
    elif op == "!=":
        expected = cmp_result != 0
    else:
        fail("Bad op '{op}'.".format(op = op))
    _assert_eq(env, versions.compare(lhs, op, rhs, **kwargs), expected, "{lhs} {op} {rhs}".format(
        lhs = lhs,
        op = op,
        rhs = rhs,
    ))

def _versions_cmp_test(ctx):
    """Unit tests for `versions.ge`."""
    env = unittest.begin(ctx)

    _test_cmp(env, 25, 25, 0)
    _test_cmp(env, 25, 42, -1)
    _test_cmp(env, 42, 25, 1)

    _test_cmp(env, "25.1", "25.1", 0)
    _test_cmp(env, "25.2", "25.1", 1)
    _test_cmp(env, "25.3", "25.4", -1)

    _test_cmp(env, "25", "25", 0)
    _test_cmp(env, "25", "25.0", -1)
    _test_cmp(env, "25.0", "25.0", 0)
    _test_cmp(env, "25.1", "25", 1)

    _test_cmp(env, "26.0", "26.0-rc1", 1)
    _test_cmp(env, "27.0-rc2", "27.0-rc1", 1)
    _test_cmp(env, "28.0-rc3", "28.0-rc4", -1)
    _test_cmp(env, "29.0-rc5", "29.0", -1)
    _test_cmp(env, "30.0-rc1", "30.0-rc1", 0)

    _test_cmp(env, "31.0-alpha", "31.0-alpha", 0)
    _test_cmp(env, "32.0-alpha", "32.0-beta", -1)
    _test_cmp(env, "33.0-alpha", "33.0-rc", -1)
    _test_cmp(env, "34.0-beta", "34.0-alpha", 1)
    _test_cmp(env, "35.0-beta", "35.0-beta", 0)
    _test_cmp(env, "36.0-beta", "36.0-rc", -1)
    _test_cmp(env, "37.0-rc", "37.0-alpha", 1)
    _test_cmp(env, "38.0-rc", "38.0-beta", 1)
    _test_cmp(env, "39.0-rc", "39.0-rc", 0)

    _test_cmp(env, "40.0-rc", "40.0-0", 1)  # Number before Text
    _test_cmp(env, "41.0-0", "41.0-rc", -1)
    _test_cmp(env, "42.0-1", "42.0-2", -1)
    _test_cmp(env, "43.0-3", "43.0-3", 0)
    _test_cmp(env, "44.0-5", "44.0-4", 1)

    _test_cmp(env, "50.0-pre+build1", "50.0-pre+build0", 0)
    _test_cmp(env, "51.0-pre+build1", "51.0-pre+build0", 0, skip_build = True)
    _test_cmp(env, "52.0-pre+build1", "52.0-pre+build0", 1, skip_build = False)
    _test_cmp(env, "53.0-pre+build2", "53.0-pre+build2", 0, skip_build = False)
    _test_cmp(env, "54.0-pre+build3", "54.0-pre+build4", -1, skip_build = False)

    _test_cmp(env, "55.0+build1", "55.0+build0", 0)
    _test_cmp(env, "56.0+build1", "56.0+build0", 0, skip_build = True)
    _test_cmp(env, "57.0+build1", "57.0+build0", 1, skip_build = False)
    _test_cmp(env, "58.0+build2", "58.0+build2", 0, skip_build = False)
    _test_cmp(env, "59.0+build3", "59.0+build4", -1, skip_build = False)

    return unittest.end(env)

def _versions_ge_test(ctx):
    """Unit tests for `versions.ge`."""
    env = unittest.begin(ctx)

    _test_op(env, 25, ">=", 25, True)
    _test_op(env, 25, ">=", 42, False)
    _test_op(env, 42, ">=", 25, True)

    _test_op(env, "25.1", ">=", "25.1", True)
    _test_op(env, "25.2", ">=", "25.1", True)
    _test_op(env, "25.3", ">=", "25.4", False)

    _test_op(env, "25", ">=", "25", True)
    _test_op(env, "25", ">=", "25.0", False)
    _test_op(env, "25.0", ">=", "25.0", True)
    _test_op(env, "25.1", ">=", "25", True)

    _test_op(env, "26.0", ">=", "26.0-rc1", True)
    _test_op(env, "27.0-rc2", ">=", "27.0-rc1", True)
    _test_op(env, "28.0-rc3", ">=", "28.0-rc4", False)
    _test_op(env, "29.0-rc5", ">=", "29.0", False)
    _test_op(env, "30.0-rc1", ">=", "30.0-rc1", True)

    _test_op(env, "31.0-alpha", ">=", "31.0-alpha", True)
    _test_op(env, "32.0-alpha", ">=", "32.0-beta", False)
    _test_op(env, "33.0-alpha", ">=", "33.0-rc", False)
    _test_op(env, "34.0-beta", ">=", "34.0-alpha", True)
    _test_op(env, "35.0-beta", ">=", "35.0-beta", True)
    _test_op(env, "36.0-beta", ">=", "36.0-rc", False)
    _test_op(env, "37.0-rc", ">=", "37.0-alpha", True)
    _test_op(env, "38.0-rc", ">=", "38.0-beta", True)
    _test_op(env, "39.0-rc", ">=", "39.0-rc", True)

    _test_op_eq_cmp(env, "55.0+build1", ">=", "55.0+build0")
    _test_op_eq_cmp(env, "56.0+build1", ">=", "56.0+build0", skip_build = True)
    _test_op_eq_cmp(env, "57.0+build1", ">=", "57.0+build0", skip_build = False)

    return unittest.end(env)

def _versions_gt_test(ctx):
    """Unit tests for `versions.gt`."""
    env = unittest.begin(ctx)

    _test_op(env, 25, ">", 25, False)
    _test_op(env, 25, ">", 42, False)
    _test_op(env, 42, ">", 25, True)

    _test_op(env, "25.1", ">", "25.1", False)
    _test_op(env, "25.2", ">", "25.1", True)
    _test_op(env, "25.3", ">", "25.4", False)

    _test_op(env, "25", ">", "25", False)
    _test_op(env, "25", ">", "25.0", False)
    _test_op(env, "25.0", ">", "25.0", False)
    _test_op(env, "25.1", ">", "25", True)

    _test_op(env, "26.0", ">", "26.0-rc1", True)
    _test_op(env, "27.0-rc2", ">", "27.0-rc1", True)
    _test_op(env, "28.0-rc3", ">", "28.0-rc4", False)
    _test_op(env, "29.0-rc5", ">", "29.0", False)
    _test_op(env, "30.0-rc1", ">", "30.0-rc1", False)

    _test_op(env, "31.0-alpha", ">", "31.0-alpha", False)
    _test_op(env, "32.0-alpha", ">", "32.0-beta", False)
    _test_op(env, "33.0-alpha", ">", "33.0-rc", False)
    _test_op(env, "34.0-beta", ">", "34.0-alpha", True)
    _test_op(env, "35.0-beta", ">", "35.0-beta", False)
    _test_op(env, "36.0-beta", ">", "36.0-rc", False)
    _test_op(env, "37.0-rc", ">", "37.0-alpha", True)
    _test_op(env, "38.0-rc", ">", "38.0-beta", True)
    _test_op(env, "39.0-rc", ">", "39.0-rc", False)

    _test_op_eq_cmp(env, "55.0+build1", ">", "55.0+build0")
    _test_op_eq_cmp(env, "56.0+build1", ">", "56.0+build0", skip_build = True)
    _test_op_eq_cmp(env, "57.0+build1", ">", "57.0+build0", skip_build = False)

    return unittest.end(env)

def _versions_le_test(ctx):
    """Unit tests for `versions.le`."""
    env = unittest.begin(ctx)

    _test_op(env, 25, "<=", 25, True)
    _test_op(env, 25, "<=", 42, True)
    _test_op(env, 42, "<=", 25, False)

    _test_op(env, "25.1", "<=", "25.1", True)
    _test_op(env, "25.2", "<=", "25.1", False)
    _test_op(env, "25.3", "<=", "25.4", True)

    _test_op(env, "25", "<=", "25", True)
    _test_op(env, "25", "<=", "25.0", True)
    _test_op(env, "25.0", "<=", "25.0", True)
    _test_op(env, "25.1", "<=", "25", False)

    _test_op(env, "26.0", "<=", "26.0-rc1", False)
    _test_op(env, "27.0-rc2", "<=", "27.0-rc1", False)
    _test_op(env, "28.0-rc3", "<=", "28.0-rc4", True)
    _test_op(env, "29.0-rc5", "<=", "29.0", True)
    _test_op(env, "30.0-rc1", "<=", "30.0-rc1", True)

    _test_op(env, "31.0-alpha", "<=", "31.0-alpha", True)
    _test_op(env, "32.0-alpha", "<=", "32.0-beta", True)
    _test_op(env, "33.0-alpha", "<=", "33.0-rc", True)
    _test_op(env, "34.0-beta", "<=", "34.0-alpha", False)
    _test_op(env, "35.0-beta", "<=", "35.0-beta", True)
    _test_op(env, "36.0-beta", "<=", "36.0-rc", True)
    _test_op(env, "37.0-rc", "<=", "37.0-alpha", False)
    _test_op(env, "38.0-rc", "<=", "38.0-beta", False)
    _test_op(env, "39.0-rc", "<=", "39.0-rc", True)

    _test_op_eq_cmp(env, "55.0+build1", "<=", "55.0+build0")
    _test_op_eq_cmp(env, "56.0+build1", "<=", "56.0+build0", skip_build = True)
    _test_op_eq_cmp(env, "57.0+build1", "<=", "57.0+build0", skip_build = False)

    return unittest.end(env)

def _versions_lt_test(ctx):
    """Unit tests for `versions.lt`."""
    env = unittest.begin(ctx)

    _test_op(env, 25, "<", 25, False)
    _test_op(env, 25, "<", 42, True)
    _test_op(env, 42, "<", 25, False)

    _test_op(env, "25.1", "<", "25.1", False)
    _test_op(env, "25.2", "<", "25.1", False)
    _test_op(env, "25.3", "<", "25.4", True)

    _test_op(env, "25", "<", "25", False)
    _test_op(env, "25", "<", "25.0", True)
    _test_op(env, "25.0", "<", "25.0", False)
    _test_op(env, "25.1", "<", "25", False)

    _test_op(env, "26.0", "<", "26.0-rc1", False)
    _test_op(env, "27.0-rc2", "<", "27.0-rc1", False)
    _test_op(env, "28.0-rc3", "<", "28.0-rc4", True)
    _test_op(env, "29.0-rc5", "<", "29.0", True)
    _test_op(env, "30.0-rc1", "<", "30.0-rc1", False)

    _test_op(env, "31.0-alpha", "<", "31.0-alpha", False)
    _test_op(env, "32.0-alpha", "<", "32.0-beta", True)
    _test_op(env, "33.0-alpha", "<", "33.0-rc", True)
    _test_op(env, "34.0-beta", "<", "34.0-alpha", False)
    _test_op(env, "35.0-beta", "<", "35.0-beta", False)
    _test_op(env, "36.0-beta", "<", "36.0-rc", True)
    _test_op(env, "37.0-rc", "<", "37.0-alpha", False)
    _test_op(env, "38.0-rc", "<", "38.0-beta", False)
    _test_op(env, "39.0-rc", "<", "39.0-rc", False)

    _test_op_eq_cmp(env, "55.0+build1", "<", "55.0+build0")
    _test_op_eq_cmp(env, "56.0+build1", "<", "56.0+build0", skip_build = True)
    _test_op_eq_cmp(env, "57.0+build1", "<", "57.0+build0", skip_build = False)

    return unittest.end(env)

def _versions_eq_test(ctx):
    """Unit tests for `versions.eq`."""
    env = unittest.begin(ctx)

    _test_op(env, 25, "==", 25, True)
    _test_op(env, 25, "==", 42, False)
    _test_op(env, 42, "==", 25, False)

    _test_op(env, "25.1", "==", "25.1", True)
    _test_op(env, "25.2", "==", "25.1", False)
    _test_op(env, "25.3", "==", "25.4", False)

    _test_op(env, "25", "==", "25", True)
    _test_op(env, "25", "==", "25.0", False)
    _test_op(env, "25.0", "==", "25.0", True)
    _test_op(env, "25.1", "==", "25", False)

    _test_op(env, "26.0", "==", "26.0-rc1", False)
    _test_op(env, "27.0-rc2", "==", "27.0-rc1", False)
    _test_op(env, "28.0-rc3", "==", "28.0-rc4", False)
    _test_op(env, "29.0-rc5", "==", "29.0", False)
    _test_op(env, "30.0-rc1", "==", "30.0-rc1", True)

    _test_op(env, "31.0-alpha", "==", "31.0-alpha", True)
    _test_op(env, "32.0-alpha", "==", "32.0-beta", False)
    _test_op(env, "33.0-alpha", "==", "33.0-rc", False)
    _test_op(env, "34.0-beta", "==", "34.0-alpha", False)
    _test_op(env, "35.0-beta", "==", "35.0-beta", True)
    _test_op(env, "36.0-beta", "==", "36.0-rc", False)
    _test_op(env, "37.0-rc", "==", "37.0-alpha", False)
    _test_op(env, "38.0-rc", "==", "38.0-beta", False)
    _test_op(env, "39.0-rc", "==", "39.0-rc", True)

    _test_op_eq_cmp(env, "55.0+build1", "==", "55.0+build0")
    _test_op_eq_cmp(env, "56.0+build1", "==", "56.0+build0", skip_build = True)
    _test_op_eq_cmp(env, "57.0+build1", "==", "57.0+build0", skip_build = False)

    return unittest.end(env)

def _versions_ne_test(ctx):
    """Unit tests for `versions.ne`."""
    env = unittest.begin(ctx)

    _test_op(env, 25, "!=", 25, False)
    _test_op(env, 25, "!=", 42, True)
    _test_op(env, 42, "!=", 25, True)

    _test_op(env, "25.1", "!=", "25.1", False)
    _test_op(env, "25.2", "!=", "25.1", True)
    _test_op(env, "25.3", "!=", "25.4", True)

    _test_op(env, "25", "!=", "25", False)
    _test_op(env, "25", "!=", "25.0", True)
    _test_op(env, "25.0", "!=", "25.0", False)
    _test_op(env, "25.1", "!=", "25", True)

    _test_op(env, "26.0", "!=", "26.0-rc1", True)
    _test_op(env, "27.0-rc2", "!=", "27.0-rc1", True)
    _test_op(env, "28.0-rc3", "!=", "28.0-rc4", True)
    _test_op(env, "29.0-rc5", "!=", "29.0", True)
    _test_op(env, "30.0-rc1", "!=", "30.0-rc1", False)

    _test_op(env, "31.0-alpha", "!=", "31.0-alpha", False)
    _test_op(env, "32.0-alpha", "!=", "32.0-beta", True)
    _test_op(env, "33.0-alpha", "!=", "33.0-rc", True)
    _test_op(env, "34.0-beta", "!=", "34.0-alpha", True)
    _test_op(env, "35.0-beta", "!=", "35.0-beta", False)
    _test_op(env, "36.0-beta", "!=", "36.0-rc", True)
    _test_op(env, "37.0-rc", "!=", "37.0-alpha", True)
    _test_op(env, "38.0-rc", "!=", "38.0-beta", True)
    _test_op(env, "39.0-rc", "!=", "39.0-rc", False)

    _test_op_eq_cmp(env, "55.0+build1", "!=", "55.0+build0")
    _test_op_eq_cmp(env, "56.0+build1", "!=", "56.0+build0", skip_build = True)
    _test_op_eq_cmp(env, "57.0+build1", "!=", "57.0+build0", skip_build = False)

    return unittest.end(env)

def _versions_parse_requirements_test(ctx):
    """Unit tests for `versions.parse_requirements`."""
    env = unittest.begin(ctx)

    _assert_eq(env, versions.parse_requirements("25"), [struct(op = "==", version = [25])])
    _assert_eq(env, versions.parse_requirements("<=26"), [struct(op = "<=", version = [26])])
    _assert_eq(env, versions.parse_requirements(">=27.1,<=28.1.2"), [
        struct(op = ">=", version = [27, 1]),
        struct(op = "<=", version = [28, 1, 2]),
    ])
    _assert_eq(env, versions.parse_requirements(" >= 29.1 , <= 29.1.2.bla "), [
        struct(op = ">=", version = [29, 1]),
        struct(op = "<=", version = [29, 1, 2, "bla"]),
    ])

    return unittest.end(env)

def _versions_check_one_requirement_test(ctx):
    """Unit tests for `versions.check_one_requirement`."""
    env = unittest.begin(ctx)

    _assert_eq(env, versions.check_one_requirement("25", "25"), True)
    _assert_eq(env, versions.check_one_requirement([26], "42"), False)
    _assert_eq(env, versions.check_one_requirement(27, ">=26"), True)
    _assert_eq(env, versions.check_one_requirement(28, "<=26"), False)
    _assert_eq(env, versions.check_one_requirement([29, 0], "<=29"), False)
    _assert_eq(env, versions.check_one_requirement([30, 0, 0], "<30"), False)
    _assert_eq(env, versions.check_one_requirement([31, 0], ">31"), True)
    _assert_eq(env, versions.check_one_requirement([31, 0], ">31"), True)
    _assert_eq(env, versions.check_one_requirement([32, 0], ">=32.0-rc1"), True)
    return unittest.end(env)

def _versions_check_all_requirements_test(ctx):
    """Unit tests for `versions.check_all_requirements`."""
    env = unittest.begin(ctx)

    _assert_eq(env, versions.check_all_requirements("25", ["25"]), True)
    _assert_eq(env, versions.check_all_requirements("33", ["25", "33", "42"]), False)
    _assert_eq(env, versions.check_all_requirements("42", ["42", "42", "42"]), True)

    _assert_eq(env, versions.check_all_requirements("34", [">=25", "!=34", "<42"]), False)
    _assert_eq(env, versions.check_all_requirements("35", [">=25", "!=34", "<42"]), True)

    _assert_eq(env, versions.check_all_requirements("36", [
        struct(op = ">=", version = 25),
        struct(op = "!=", version = 34),
        struct(op = "<", version = 42),
    ]), True)

    _assert_eq(env, versions.check_all_requirements("37", [
        struct(op = ">=", version = 25),
        struct(op = "!=", version = 37),
        struct(op = "<", version = 42),
    ]), False)

    return unittest.end(env)

versions_parse_test = unittest.make(_versions_parse_test)
versions_ge_test = unittest.make(_versions_ge_test)
versions_gt_test = unittest.make(_versions_gt_test)
versions_le_test = unittest.make(_versions_le_test)
versions_lt_test = unittest.make(_versions_lt_test)
versions_eq_test = unittest.make(_versions_eq_test)
versions_ne_test = unittest.make(_versions_ne_test)
versions_cmp_test = unittest.make(_versions_cmp_test)
versions_parse_requirements_test = unittest.make(_versions_parse_requirements_test)
versions_check_one_requirement_test = unittest.make(_versions_check_one_requirement_test)
versions_check_all_requirements_test = unittest.make(_versions_check_all_requirements_test)

def versions_test_suite():
    """Creates the test targets and test suite for paths.bzl tests."""
    unittest.suite(
        "versions_tests",
        versions_parse_test,
        versions_ge_test,
        versions_gt_test,
        versions_le_test,
        versions_lt_test,
        versions_eq_test,
        versions_ne_test,
        versions_cmp_test,
        versions_parse_requirements_test,
        versions_check_one_requirement_test,
        versions_check_all_requirements_test,
    )
