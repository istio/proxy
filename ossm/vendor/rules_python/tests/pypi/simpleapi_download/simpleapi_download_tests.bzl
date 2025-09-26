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

""

load("@rules_testing//lib:test_suite.bzl", "test_suite")
load("//python/private/pypi:simpleapi_download.bzl", "simpleapi_download", "strip_empty_path_segments")  # buildifier: disable=bzl-visibility

_tests = []

def _test_simple(env):
    calls = []

    def read_simpleapi(ctx, url, attr, cache, get_auth, block):
        _ = ctx  # buildifier: disable=unused-variable
        _ = attr
        _ = cache
        _ = get_auth
        env.expect.that_bool(block).equals(False)
        calls.append(url)
        if "foo" in url and "main" in url:
            return struct(
                output = "",
                success = False,
            )
        else:
            return struct(
                output = "data from {}".format(url),
                success = True,
            )

    contents = simpleapi_download(
        ctx = struct(
            os = struct(environ = {}),
            report_progress = lambda _: None,
        ),
        attr = struct(
            index_url_overrides = {},
            index_url = "main",
            extra_index_urls = ["extra"],
            sources = ["foo", "bar", "baz"],
            envsubst = [],
        ),
        cache = {},
        parallel_download = True,
        read_simpleapi = read_simpleapi,
    )

    env.expect.that_collection(calls).contains_exactly([
        "extra/foo/",
        "main/bar/",
        "main/baz/",
        "main/foo/",
    ])
    env.expect.that_dict(contents).contains_exactly({
        "bar": "data from main/bar/",
        "baz": "data from main/baz/",
        "foo": "data from extra/foo/",
    })

_tests.append(_test_simple)

def _test_fail(env):
    calls = []
    fails = []

    def read_simpleapi(ctx, url, attr, cache, get_auth, block):
        _ = ctx  # buildifier: disable=unused-variable
        _ = attr
        _ = cache
        _ = get_auth
        env.expect.that_bool(block).equals(False)
        calls.append(url)
        if "foo" in url:
            return struct(
                output = "",
                success = False,
            )
        if "bar" in url:
            return struct(
                output = "",
                success = False,
            )
        else:
            return struct(
                output = "data from {}".format(url),
                success = True,
            )

    simpleapi_download(
        ctx = struct(
            os = struct(environ = {}),
            report_progress = lambda _: None,
        ),
        attr = struct(
            index_url_overrides = {
                "foo": "invalid",
            },
            index_url = "main",
            extra_index_urls = ["extra"],
            sources = ["foo", "bar", "baz"],
            envsubst = [],
        ),
        cache = {},
        parallel_download = True,
        read_simpleapi = read_simpleapi,
        _fail = fails.append,
    )

    env.expect.that_collection(fails).contains_exactly([
        """
Failed to download metadata of the following packages from urls:
{
    "foo": "invalid",
    "bar": ["main", "extra"],
}

If you would like to skip downloading metadata for these packages please add 'simpleapi_skip=[
    "foo",
    "bar",
]' to your 'pip.parse' call.
""",
    ])
    env.expect.that_collection(calls).contains_exactly([
        "invalid/foo/",
        "main/bar/",
        "main/baz/",
        "invalid/foo/",
        "extra/bar/",
    ])

_tests.append(_test_fail)

def _test_download_url(env):
    downloads = {}

    def download(url, output, **kwargs):
        _ = kwargs  # buildifier: disable=unused-variable
        downloads[url[0]] = output
        return struct(success = True)

    simpleapi_download(
        ctx = struct(
            os = struct(environ = {}),
            download = download,
            report_progress = lambda _: None,
            read = lambda i: "contents of " + i,
            path = lambda i: "path/for/" + i,
        ),
        attr = struct(
            index_url_overrides = {},
            index_url = "https://example.com/main/simple/",
            extra_index_urls = [],
            sources = ["foo", "bar", "baz"],
            envsubst = [],
        ),
        cache = {},
        parallel_download = False,
        get_auth = lambda ctx, urls, ctx_attr: struct(),
    )

    env.expect.that_dict(downloads).contains_exactly({
        "https://example.com/main/simple/bar/": "path/for/https___example_com_main_simple_bar.html",
        "https://example.com/main/simple/baz/": "path/for/https___example_com_main_simple_baz.html",
        "https://example.com/main/simple/foo/": "path/for/https___example_com_main_simple_foo.html",
    })

_tests.append(_test_download_url)

def _test_download_url_parallel(env):
    downloads = {}

    def download(url, output, **kwargs):
        _ = kwargs  # buildifier: disable=unused-variable
        downloads[url[0]] = output
        return struct(wait = lambda: struct(success = True))

    simpleapi_download(
        ctx = struct(
            os = struct(environ = {}),
            download = download,
            report_progress = lambda _: None,
            read = lambda i: "contents of " + i,
            path = lambda i: "path/for/" + i,
        ),
        attr = struct(
            index_url_overrides = {},
            index_url = "https://example.com/main/simple/",
            extra_index_urls = [],
            sources = ["foo", "bar", "baz"],
            envsubst = [],
        ),
        cache = {},
        parallel_download = True,
        get_auth = lambda ctx, urls, ctx_attr: struct(),
    )

    env.expect.that_dict(downloads).contains_exactly({
        "https://example.com/main/simple/bar/": "path/for/https___example_com_main_simple_bar.html",
        "https://example.com/main/simple/baz/": "path/for/https___example_com_main_simple_baz.html",
        "https://example.com/main/simple/foo/": "path/for/https___example_com_main_simple_foo.html",
    })

_tests.append(_test_download_url_parallel)

def _test_download_envsubst_url(env):
    downloads = {}

    def download(url, output, **kwargs):
        _ = kwargs  # buildifier: disable=unused-variable
        downloads[url[0]] = output
        return struct(success = True)

    simpleapi_download(
        ctx = struct(
            os = struct(environ = {"INDEX_URL": "https://example.com/main/simple/"}),
            download = download,
            report_progress = lambda _: None,
            read = lambda i: "contents of " + i,
            path = lambda i: "path/for/" + i,
        ),
        attr = struct(
            index_url_overrides = {},
            index_url = "$INDEX_URL",
            extra_index_urls = [],
            sources = ["foo", "bar", "baz"],
            envsubst = ["INDEX_URL"],
        ),
        cache = {},
        parallel_download = False,
        get_auth = lambda ctx, urls, ctx_attr: struct(),
    )

    env.expect.that_dict(downloads).contains_exactly({
        "https://example.com/main/simple/bar/": "path/for/~index_url~_bar.html",
        "https://example.com/main/simple/baz/": "path/for/~index_url~_baz.html",
        "https://example.com/main/simple/foo/": "path/for/~index_url~_foo.html",
    })

_tests.append(_test_download_envsubst_url)

def _test_strip_empty_path_segments(env):
    env.expect.that_str(strip_empty_path_segments("no/scheme//is/unchanged")).equals("no/scheme//is/unchanged")
    env.expect.that_str(strip_empty_path_segments("scheme://with/no/empty/segments")).equals("scheme://with/no/empty/segments")
    env.expect.that_str(strip_empty_path_segments("scheme://with//empty/segments")).equals("scheme://with/empty/segments")
    env.expect.that_str(strip_empty_path_segments("scheme://with///multiple//empty/segments")).equals("scheme://with/multiple/empty/segments")
    env.expect.that_str(strip_empty_path_segments("scheme://with//trailing/slash/")).equals("scheme://with/trailing/slash/")
    env.expect.that_str(strip_empty_path_segments("scheme://with/trailing/slashes///")).equals("scheme://with/trailing/slashes/")

_tests.append(_test_strip_empty_path_segments)

def simpleapi_download_test_suite(name):
    """Create the test suite.

    Args:
        name: the name of the test suite
    """
    test_suite(name = name, basic_tests = _tests)
