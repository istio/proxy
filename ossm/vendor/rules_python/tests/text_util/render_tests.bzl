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
load("//python/private:text_util.bzl", "render")  # buildifier: disable=bzl-visibility

_tests = []

def _test_render_alias(env):
    tests = [
        struct(
            args = dict(
                name = "foo",
                actual = repr("bar"),
            ),
            want = [
                "alias(",
                '    name = "foo",',
                '    actual = "bar",',
                ")",
            ],
        ),
        struct(
            args = dict(
                name = "foo",
                actual = repr("bar"),
                visibility = ["//:__pkg__"],
            ),
            want = [
                "alias(",
                '    name = "foo",',
                '    actual = "bar",',
                '    visibility = ["//:__pkg__"],',
                ")",
            ],
        ),
    ]
    for test in tests:
        got = render.alias(**test.args)
        env.expect.that_str(got).equals("\n".join(test.want).strip())

_tests.append(_test_render_alias)

def _test_render_tuple_dict(env):
    got = render.dict(
        {
            ("foo", "bar"): "baz",
            ("foo",): "bar",
        },
        key_repr = render.tuple,
    )
    env.expect.that_str(got).equals("""\
{
    (
        "foo",
        "bar",
    ): "baz",
    ("foo",): "bar",
}""")

_tests.append(_test_render_tuple_dict)

def render_test_suite(name):
    """Create the test suite.

    Args:
        name: the name of the test suite
    """
    test_suite(name = name, basic_tests = _tests)
