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
load("@rules_testing//lib:truth.bzl", "subjects")
load("//python/private/pypi:parse_simpleapi_html.bzl", "parse_simpleapi_html")  # buildifier: disable=bzl-visibility

_tests = []

def _generate_html(*items):
    return """\
<html>
  <head>
    <meta name="pypi:repository-version" content="1.1">
    <title>Links for foo</title>
  </head>
  <body>
    <h1>Links for cengal</h1>
{}
</body>
</html>
""".format(
        "\n".join([
            "<a {}>{}</a><br />".format(
                " ".join(item.attrs),
                item.filename,
            )
            for item in items
        ]),
    )

def _test_sdist(env):
    # buildifier: disable=unsorted-dict-items
    tests = [
        (
            struct(
                attrs = [
                    'href="https://example.org/full-url/foo-0.0.1.tar.gz#sha256=deadbeefasource"',
                    'data-requires-python="&gt;=3.7"',
                ],
                filename = "foo-0.0.1.tar.gz",
                url = "ignored",
            ),
            struct(
                filename = "foo-0.0.1.tar.gz",
                sha256 = "deadbeefasource",
                url = "https://example.org/full-url/foo-0.0.1.tar.gz",
                yanked = False,
            ),
        ),
        (
            struct(
                attrs = [
                    'href="https://example.org/full-url/foo-0.0.1.tar.gz#sha256=deadbeefasource"',
                    'data-requires-python=">=3.7"',
                ],
                filename = "foo-0.0.1.tar.gz",
                url = "ignored",
            ),
            struct(
                filename = "foo-0.0.1.tar.gz",
                sha256 = "deadbeefasource",
                url = "https://example.org/full-url/foo-0.0.1.tar.gz",
                yanked = False,
            ),
        ),
    ]

    for (input, want) in tests:
        html = _generate_html(input)
        got = parse_simpleapi_html(url = input.url, content = html)
        env.expect.that_collection(got.sdists).has_size(1)
        env.expect.that_collection(got.whls).has_size(0)
        if not got:
            fail("expected at least one element, but did not get anything from:\n{}".format(html))

        actual = env.expect.that_struct(
            got.sdists[want.sha256],
            attrs = dict(
                filename = subjects.str,
                sha256 = subjects.str,
                url = subjects.str,
                yanked = subjects.bool,
            ),
        )
        actual.filename().equals(want.filename)
        actual.sha256().equals(want.sha256)
        actual.url().equals(want.url)
        actual.yanked().equals(want.yanked)

_tests.append(_test_sdist)

def _test_whls(env):
    # buildifier: disable=unsorted-dict-items
    tests = [
        (
            struct(
                attrs = [
                    'href="https://example.org/full-url/foo-0.0.2-cp310-cp310-manylinux_2_17_x86_64.manylinux2014_x86_64.whl#sha256=deadbeef"',
                    'data-requires-python="&gt;=3.7"',
                    'data-dist-info-metadata="sha256=deadb00f"',
                    'data-core-metadata="sha256=deadb00f"',
                ],
                filename = "foo-0.0.2-cp310-cp310-manylinux_2_17_x86_64.manylinux2014_x86_64.whl",
                url = "ignored",
            ),
            struct(
                filename = "foo-0.0.2-cp310-cp310-manylinux_2_17_x86_64.manylinux2014_x86_64.whl",
                metadata_sha256 = "deadb00f",
                metadata_url = "https://example.org/full-url/foo-0.0.2-cp310-cp310-manylinux_2_17_x86_64.manylinux2014_x86_64.whl.metadata",
                sha256 = "deadbeef",
                url = "https://example.org/full-url/foo-0.0.2-cp310-cp310-manylinux_2_17_x86_64.manylinux2014_x86_64.whl",
                yanked = False,
            ),
        ),
        (
            struct(
                attrs = [
                    'href="https://example.org/full-url/foo-0.0.2-cp310-cp310-manylinux_2_17_x86_64.manylinux2014_x86_64.whl#sha256=deadbeef"',
                    'data-requires-python=">=3.7"',
                    'data-dist-info-metadata="sha256=deadb00f"',
                    'data-core-metadata="sha256=deadb00f"',
                ],
                filename = "foo-0.0.2-cp310-cp310-manylinux_2_17_x86_64.manylinux2014_x86_64.whl",
                url = "ignored",
            ),
            struct(
                filename = "foo-0.0.2-cp310-cp310-manylinux_2_17_x86_64.manylinux2014_x86_64.whl",
                metadata_sha256 = "deadb00f",
                metadata_url = "https://example.org/full-url/foo-0.0.2-cp310-cp310-manylinux_2_17_x86_64.manylinux2014_x86_64.whl.metadata",
                sha256 = "deadbeef",
                url = "https://example.org/full-url/foo-0.0.2-cp310-cp310-manylinux_2_17_x86_64.manylinux2014_x86_64.whl",
                yanked = False,
            ),
        ),
        (
            struct(
                attrs = [
                    'href="https://example.org/full-url/foo-0.0.2-cp310-cp310-manylinux_2_17_x86_64.manylinux2014_x86_64.whl#sha256=deadbeef"',
                    'data-requires-python="&gt;=3.7"',
                    'data-core-metadata="sha256=deadb00f"',
                ],
                filename = "foo-0.0.2-cp310-cp310-manylinux_2_17_x86_64.manylinux2014_x86_64.whl",
                url = "ignored",
            ),
            struct(
                filename = "foo-0.0.2-cp310-cp310-manylinux_2_17_x86_64.manylinux2014_x86_64.whl",
                metadata_sha256 = "deadb00f",
                metadata_url = "https://example.org/full-url/foo-0.0.2-cp310-cp310-manylinux_2_17_x86_64.manylinux2014_x86_64.whl.metadata",
                sha256 = "deadbeef",
                url = "https://example.org/full-url/foo-0.0.2-cp310-cp310-manylinux_2_17_x86_64.manylinux2014_x86_64.whl",
                yanked = False,
            ),
        ),
        (
            struct(
                attrs = [
                    'href="https://example.org/full-url/foo-0.0.2-cp310-cp310-manylinux_2_17_x86_64.manylinux2014_x86_64.whl#sha256=deadbeef"',
                    'data-requires-python="&gt;=3.7"',
                    'data-dist-info-metadata="sha256=deadb00f"',
                ],
                filename = "foo-0.0.2-cp310-cp310-manylinux_2_17_x86_64.manylinux2014_x86_64.whl",
                url = "ignored",
            ),
            struct(
                filename = "foo-0.0.2-cp310-cp310-manylinux_2_17_x86_64.manylinux2014_x86_64.whl",
                metadata_sha256 = "deadb00f",
                metadata_url = "https://example.org/full-url/foo-0.0.2-cp310-cp310-manylinux_2_17_x86_64.manylinux2014_x86_64.whl.metadata",
                sha256 = "deadbeef",
                url = "https://example.org/full-url/foo-0.0.2-cp310-cp310-manylinux_2_17_x86_64.manylinux2014_x86_64.whl",
                yanked = False,
            ),
        ),
        (
            struct(
                attrs = [
                    'href="https://example.org/full-url/foo-0.0.2-cp310-cp310-manylinux_2_17_x86_64.manylinux2014_x86_64.whl#sha256=deadbeef"',
                    'data-requires-python="&gt;=3.7"',
                ],
                filename = "foo-0.0.2-cp310-cp310-manylinux_2_17_x86_64.manylinux2014_x86_64.whl",
                url = "ignored",
            ),
            struct(
                filename = "foo-0.0.2-cp310-cp310-manylinux_2_17_x86_64.manylinux2014_x86_64.whl",
                metadata_sha256 = "",
                metadata_url = "",
                sha256 = "deadbeef",
                url = "https://example.org/full-url/foo-0.0.2-cp310-cp310-manylinux_2_17_x86_64.manylinux2014_x86_64.whl",
                yanked = False,
            ),
        ),
        (
            struct(
                attrs = [
                    'href="../../foo-0.0.2-cp310-cp310-manylinux_2_17_x86_64.manylinux2014_x86_64.whl#sha256=deadbeef"',
                    'data-requires-python="&gt;=3.7"',
                    'data-dist-info-metadata="sha256=deadb00f"',
                ],
                filename = "foo-0.0.2-cp310-cp310-manylinux_2_17_x86_64.manylinux2014_x86_64.whl",
                url = "https://example.org/python-wheels/bar/foo/",
            ),
            struct(
                filename = "foo-0.0.2-cp310-cp310-manylinux_2_17_x86_64.manylinux2014_x86_64.whl",
                metadata_sha256 = "deadb00f",
                metadata_url = "https://example.org/python-wheels/foo-0.0.2-cp310-cp310-manylinux_2_17_x86_64.manylinux2014_x86_64.whl.metadata",
                sha256 = "deadbeef",
                url = "https://example.org/python-wheels/foo-0.0.2-cp310-cp310-manylinux_2_17_x86_64.manylinux2014_x86_64.whl",
                yanked = False,
            ),
        ),
        (
            struct(
                attrs = [
                    'href="/whl/torch-2.0.0-cp38-cp38-manylinux2014_aarch64.whl#sha256=deadbeef"',
                ],
                filename = "torch-2.0.0-cp38-cp38-manylinux2014_aarch64.whl",
                url = "https://download.pytorch.org/whl/cpu/torch",
            ),
            struct(
                filename = "torch-2.0.0-cp38-cp38-manylinux2014_aarch64.whl",
                metadata_sha256 = "",
                metadata_url = "",
                sha256 = "deadbeef",
                url = "https://download.pytorch.org/whl/torch-2.0.0-cp38-cp38-manylinux2014_aarch64.whl",
                yanked = False,
            ),
        ),
        (
            struct(
                attrs = [
                    'href="/whl/torch-2.0.0-cp38-cp38-manylinux2014_aarch64.whl#sha256=notdeadbeef"',
                ],
                filename = "torch-2.0.0-cp38-cp38-manylinux2014_aarch64.whl",
                url = "http://download.pytorch.org/whl/cpu/torch",
            ),
            struct(
                filename = "torch-2.0.0-cp38-cp38-manylinux2014_aarch64.whl",
                metadata_sha256 = "",
                metadata_url = "",
                sha256 = "notdeadbeef",
                url = "http://download.pytorch.org/whl/torch-2.0.0-cp38-cp38-manylinux2014_aarch64.whl",
                yanked = False,
            ),
        ),
        (
            struct(
                attrs = [
                    'href="1.0.0/mypy_extensions-1.0.0-py3-none-any.whl#sha256=deadbeef"',
                ],
                filename = "mypy_extensions-1.0.0-py3-none-any.whl",
                url = "https://example.org/simple/mypy_extensions",
            ),
            struct(
                filename = "mypy_extensions-1.0.0-py3-none-any.whl",
                metadata_sha256 = "",
                metadata_url = "",
                sha256 = "deadbeef",
                url = "https://example.org/simple/mypy_extensions/1.0.0/mypy_extensions-1.0.0-py3-none-any.whl",
                yanked = False,
            ),
        ),
        (
            struct(
                attrs = [
                    'href="unknown://example.com/mypy_extensions-1.0.0-py3-none-any.whl#sha256=deadbeef"',
                ],
                filename = "mypy_extensions-1.0.0-py3-none-any.whl",
                url = "https://example.org/simple/mypy_extensions",
            ),
            struct(
                filename = "mypy_extensions-1.0.0-py3-none-any.whl",
                metadata_sha256 = "",
                metadata_url = "",
                sha256 = "deadbeef",
                url = "https://example.org/simple/mypy_extensions/unknown://example.com/mypy_extensions-1.0.0-py3-none-any.whl",
                yanked = False,
            ),
        ),
    ]

    for (input, want) in tests:
        html = _generate_html(input)
        got = parse_simpleapi_html(url = input.url, content = html)
        env.expect.that_collection(got.sdists).has_size(0)
        env.expect.that_collection(got.whls).has_size(1)
        if not got:
            fail("expected at least one element, but did not get anything from:\n{}".format(html))

        actual = env.expect.that_struct(
            got.whls[want.sha256],
            attrs = dict(
                filename = subjects.str,
                metadata_sha256 = subjects.str,
                metadata_url = subjects.str,
                sha256 = subjects.str,
                url = subjects.str,
                yanked = subjects.bool,
            ),
        )
        actual.filename().equals(want.filename)
        actual.metadata_sha256().equals(want.metadata_sha256)
        actual.metadata_url().equals(want.metadata_url)
        actual.sha256().equals(want.sha256)
        actual.url().equals(want.url)
        actual.yanked().equals(want.yanked)

_tests.append(_test_whls)

def parse_simpleapi_html_test_suite(name):
    """Create the test suite.

    Args:
        name: the name of the test suite
    """
    test_suite(name = name, basic_tests = _tests)
