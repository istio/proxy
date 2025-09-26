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
load("//python/private/pypi:index_sources.bzl", "index_sources")  # buildifier: disable=bzl-visibility

_tests = []

def _test_no_simple_api_sources(env):
    inputs = {
        "foo @ git+https://github.com/org/foo.git@deadbeef": struct(
            requirement = "foo @ git+https://github.com/org/foo.git@deadbeef",
            requirement_line = "foo @ git+https://github.com/org/foo.git@deadbeef",
            marker = "",
            url = "git+https://github.com/org/foo.git@deadbeef",
            shas = [],
            version = "",
            filename = "",
        ),
        "foo==0.0.1": struct(
            requirement = "foo==0.0.1",
            requirement_line = "foo==0.0.1",
            marker = "",
            url = "",
            version = "0.0.1",
            filename = "",
        ),
        "foo==0.0.1 @ https://someurl.org": struct(
            requirement = "foo==0.0.1 @ https://someurl.org",
            requirement_line = "foo==0.0.1 @ https://someurl.org",
            marker = "",
            url = "https://someurl.org",
            version = "0.0.1",
            filename = "",
        ),
        "foo==0.0.1 @ https://someurl.org/package.whl": struct(
            requirement = "foo==0.0.1",
            requirement_line = "foo==0.0.1 @ https://someurl.org/package.whl",
            marker = "",
            url = "https://someurl.org/package.whl",
            version = "0.0.1",
            filename = "package.whl",
        ),
        "foo==0.0.1 @ https://someurl.org/package.whl --hash=sha256:deadbeef": struct(
            requirement = "foo==0.0.1",
            requirement_line = "foo==0.0.1 @ https://someurl.org/package.whl --hash=sha256:deadbeef",
            marker = "",
            url = "https://someurl.org/package.whl",
            shas = ["deadbeef"],
            version = "0.0.1",
            filename = "package.whl",
        ),
        "foo==0.0.1 @ https://someurl.org/package.whl; python_version < \"2.7\"\\    --hash=sha256:deadbeef": struct(
            requirement = "foo==0.0.1",
            requirement_line = "foo==0.0.1 @ https://someurl.org/package.whl --hash=sha256:deadbeef",
            marker = "python_version < \"2.7\"",
            url = "https://someurl.org/package.whl",
            shas = ["deadbeef"],
            version = "0.0.1",
            filename = "package.whl",
        ),
        "foo[extra] @ https://example.org/foo-1.0.tar.gz --hash=sha256:deadbe0f": struct(
            # NOTE @aignas 2025-08-03: we need to ensure that sdists continue working
            # when we are using pip to install them even if the experimental_index_url
            # code path is used.
            requirement = "foo[extra] @ https://example.org/foo-1.0.tar.gz --hash=sha256:deadbe0f",
            requirement_line = "foo[extra] @ https://example.org/foo-1.0.tar.gz --hash=sha256:deadbe0f",
            marker = "",
            url = "https://example.org/foo-1.0.tar.gz",
            shas = ["deadbe0f"],
            version = "",
            filename = "foo-1.0.tar.gz",
        ),
        "torch @ https://download.pytorch.org/whl/cpu/torch-2.6.0%2Bcpu-cp311-cp311-linux_x86_64.whl#sha256=deadbeef": struct(
            requirement = "torch",
            requirement_line = "torch @ https://download.pytorch.org/whl/cpu/torch-2.6.0%2Bcpu-cp311-cp311-linux_x86_64.whl#sha256=deadbeef",
            marker = "",
            url = "https://download.pytorch.org/whl/cpu/torch-2.6.0%2Bcpu-cp311-cp311-linux_x86_64.whl",
            shas = ["deadbeef"],
            version = "",
            filename = "torch-2.6.0+cpu-cp311-cp311-linux_x86_64.whl",
        ),
    }
    for input, want in inputs.items():
        got = index_sources(input)
        env.expect.that_collection(got.shas).contains_exactly(want.shas if hasattr(want, "shas") else [])
        env.expect.that_str(got.version).equals(want.version)
        env.expect.that_str(got.requirement).equals(want.requirement)
        env.expect.that_str(got.requirement_line).equals(got.requirement_line)
        env.expect.that_str(got.marker).equals(want.marker)
        env.expect.that_str(got.url).equals(want.url)
        env.expect.that_str(got.filename).equals(want.filename)

_tests.append(_test_no_simple_api_sources)

def _test_simple_api_sources(env):
    tests = {
        "foo==0.0.2 --hash=sha256:deafbeef    --hash=sha256:deadbeef": struct(
            shas = [
                "deadbeef",
                "deafbeef",
            ],
            marker = "",
            requirement = "foo==0.0.2",
            requirement_line = "foo==0.0.2 --hash=sha256:deafbeef --hash=sha256:deadbeef",
            url = "",
        ),
        "foo[extra]==0.0.2; (python_version < 2.7 or extra == \"@\") --hash=sha256:deafbeef    --hash=sha256:deadbeef": struct(
            shas = [
                "deadbeef",
                "deafbeef",
            ],
            marker = "(python_version < 2.7 or extra == \"@\")",
            requirement = "foo[extra]==0.0.2",
            requirement_line = "foo[extra]==0.0.2 --hash=sha256:deafbeef --hash=sha256:deadbeef",
            url = "",
        ),
    }
    for input, want in tests.items():
        got = index_sources(input)
        env.expect.that_collection(got.shas).contains_exactly(want.shas)
        env.expect.that_str(got.version).equals("0.0.2")
        env.expect.that_str(got.requirement).equals(want.requirement)
        env.expect.that_str(got.requirement_line).equals(want.requirement_line)
        env.expect.that_str(got.marker).equals(want.marker)
        env.expect.that_str(got.url).equals(want.url)

_tests.append(_test_simple_api_sources)

def index_sources_test_suite(name):
    """Create the test suite.

    Args:
        name: the name of the test suite
    """
    test_suite(name = name, basic_tests = _tests)
