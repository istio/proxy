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

"""
A file that houses private functions used in the `bzlmod` extension with the same name.
"""

load("@bazel_features//:features.bzl", "bazel_features")
load("//python/private:auth.bzl", _get_auth = "get_auth")
load("//python/private:envsubst.bzl", "envsubst")
load("//python/private:normalize_name.bzl", "normalize_name")
load("//python/private:text_util.bzl", "render")
load(":parse_simpleapi_html.bzl", "parse_simpleapi_html")

def simpleapi_download(
        ctx,
        *,
        attr,
        cache,
        parallel_download = True,
        read_simpleapi = None,
        get_auth = None,
        _fail = fail):
    """Download Simple API HTML.

    Args:
        ctx: The module_ctx or repository_ctx.
        attr: Contains the parameters for the download. They are grouped into a
          struct for better clarity. It must have attributes:
           * index_url: str, the index.
           * index_url_overrides: dict[str, str], the index overrides for
             separate packages.
           * extra_index_urls: Extra index URLs that will be looked up after
             the main is looked up.
           * sources: list[str], the sources to download things for. Each value is
             the contents of requirements files.
           * envsubst: list[str], the envsubst vars for performing substitution in index url.
           * netrc: The netrc parameter for ctx.download, see http_file for docs.
           * auth_patterns: The auth_patterns parameter for ctx.download, see
               http_file for docs.
        cache: A dictionary that can be used as a cache between calls during a
            single evaluation of the extension. We use a dictionary as a cache
            so that we can reuse calls to the simple API when evaluating the
            extension. Using the canonical_id parameter of the module_ctx would
            deposit the simple API responses to the bazel cache and that is
            undesirable because additions to the PyPI index would not be
            reflected when re-evaluating the extension unless we do
            `bazel clean --expunge`.
        parallel_download: A boolean to enable usage of bazel 7.1 non-blocking downloads.
        read_simpleapi: a function for reading and parsing of the SimpleAPI contents.
            Used in tests.
        get_auth: A function to get auth information passed to read_simpleapi. Used in tests.
        _fail: a function to print a failure. Used in tests.

    Returns:
        dict of pkg name to the parsed HTML contents - a list of structs.
    """
    index_url_overrides = {
        normalize_name(p): i
        for p, i in (attr.index_url_overrides or {}).items()
    }

    download_kwargs = {}
    if bazel_features.external_deps.download_has_block_param:
        download_kwargs["block"] = not parallel_download

    # NOTE @aignas 2024-03-31: we are not merging results from multiple indexes
    # to replicate how `pip` would handle this case.
    contents = {}
    index_urls = [attr.index_url] + attr.extra_index_urls
    read_simpleapi = read_simpleapi or _read_simpleapi

    found_on_index = {}
    warn_overrides = False
    ctx.report_progress("Fetch package lists from PyPI index")
    for i, index_url in enumerate(index_urls):
        if i != 0:
            # Warn the user about a potential fix for the overrides
            warn_overrides = True

        async_downloads = {}
        sources = [pkg for pkg in attr.sources if pkg not in found_on_index]
        for pkg in sources:
            pkg_normalized = normalize_name(pkg)
            result = read_simpleapi(
                ctx = ctx,
                url = "{}/{}/".format(
                    index_url_overrides.get(pkg_normalized, index_url).rstrip("/"),
                    pkg,
                ),
                attr = attr,
                cache = cache,
                get_auth = get_auth,
                **download_kwargs
            )
            if hasattr(result, "wait"):
                # We will process it in a separate loop:
                async_downloads[pkg] = struct(
                    pkg_normalized = pkg_normalized,
                    wait = result.wait,
                )
            elif result.success:
                contents[pkg_normalized] = result.output
                found_on_index[pkg] = index_url

        if not async_downloads:
            continue

        # If we use `block` == False, then we need to have a second loop that is
        # collecting all of the results as they were being downloaded in parallel.
        for pkg, download in async_downloads.items():
            result = download.wait()

            if result.success:
                contents[download.pkg_normalized] = result.output
                found_on_index[pkg] = index_url

    failed_sources = [pkg for pkg in attr.sources if pkg not in found_on_index]
    if failed_sources:
        pkg_index_urls = {
            pkg: index_url_overrides.get(
                normalize_name(pkg),
                index_urls,
            )
            for pkg in failed_sources
        }

        _fail(
            """
Failed to download metadata of the following packages from urls:
{pkg_index_urls}

If you would like to skip downloading metadata for these packages please add 'simpleapi_skip={failed_sources}' to your 'pip.parse' call.
""".format(
                pkg_index_urls = render.dict(pkg_index_urls),
                failed_sources = render.list(failed_sources),
            ),
        )
        return None

    if warn_overrides:
        index_url_overrides = {
            pkg: found_on_index[pkg]
            for pkg in attr.sources
            if found_on_index[pkg] != attr.index_url
        }

        if index_url_overrides:
            # buildifier: disable=print
            print("You can use the following `index_url_overrides` to avoid the 404 warnings:\n{}".format(
                render.dict(index_url_overrides),
            ))

    return contents

def _read_simpleapi(ctx, url, attr, cache, get_auth = None, **download_kwargs):
    """Read SimpleAPI.

    Args:
        ctx: The module_ctx or repository_ctx.
        url: str, the url parameter that can be passed to ctx.download.
        attr: The attribute that contains necessary info for downloading. The
          following attributes must be present:
           * envsubst: The envsubst values for performing substitutions in the URL.
           * netrc: The netrc parameter for ctx.download, see http_file for docs.
           * auth_patterns: The auth_patterns parameter for ctx.download, see
               http_file for docs.
        cache: A dict for storing the results.
        get_auth: A function to get auth information. Used in tests.
        **download_kwargs: Any extra params to ctx.download.
            Note that output and auth will be passed for you.

    Returns:
        A similar object to what `download` would return except that in result.out
        will be the parsed simple api contents.
    """
    # NOTE @aignas 2024-03-31: some of the simple APIs use relative URLs for
    # the whl location and we cannot handle multiple URLs at once by passing
    # them to ctx.download if we want to correctly handle the relative URLs.
    # TODO: Add a test that env subbed index urls do not leak into the lock file.

    real_url = strip_empty_path_segments(envsubst(
        url,
        attr.envsubst,
        ctx.getenv if hasattr(ctx, "getenv") else ctx.os.environ.get,
    ))

    cache_key = real_url
    if cache_key in cache:
        return struct(success = True, output = cache[cache_key])

    output_str = envsubst(
        url,
        attr.envsubst,
        # Use env names in the subst values - this will be unique over
        # the lifetime of the execution of this function and we also use
        # `~` as the separator to ensure that we don't get clashes.
        {e: "~{}~".format(e) for e in attr.envsubst}.get,
    )

    # Transform the URL into a valid filename
    for char in [".", ":", "/", "\\", "-"]:
        output_str = output_str.replace(char, "_")

    output = ctx.path(output_str.strip("_").lower() + ".html")

    get_auth = get_auth or _get_auth

    # NOTE: this may have block = True or block = False in the download_kwargs
    download = ctx.download(
        url = [real_url],
        output = output,
        auth = get_auth(ctx, [real_url], ctx_attr = attr),
        allow_fail = True,
        **download_kwargs
    )

    if download_kwargs.get("block") == False:
        # Simulate the same API as ctx.download has
        return struct(
            wait = lambda: _read_index_result(ctx, download.wait(), output, real_url, cache, cache_key),
        )

    return _read_index_result(ctx, download, output, real_url, cache, cache_key)

def strip_empty_path_segments(url):
    """Removes empty path segments from a URL. Does nothing for urls with no scheme.

    Public only for testing.

    Args:
        url: The url to remove empty path segments from

    Returns:
        The url with empty path segments removed and any trailing slash preserved.
        If the url had no scheme it is returned unchanged.
    """
    scheme, _, rest = url.partition("://")
    if rest == "":
        return url
    stripped = "/".join([p for p in rest.split("/") if p])
    if url.endswith("/"):
        return "{}://{}/".format(scheme, stripped)
    else:
        return "{}://{}".format(scheme, stripped)

def _read_index_result(ctx, result, output, url, cache, cache_key):
    if not result.success:
        return struct(success = False)

    content = ctx.read(output)

    output = parse_simpleapi_html(url = url, content = content)
    if output:
        cache.setdefault(cache_key, output)
        return struct(success = True, output = output, cache_key = cache_key)
    else:
        return struct(success = False)
