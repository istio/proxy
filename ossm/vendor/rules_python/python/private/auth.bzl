# Copyright 2022 The Bazel Authors. All rights reserved.
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

"""Helpers copied from http_file source to be reused here.

The implementation below is copied directly from Bazel's implementation of `http_archive`.
Accordingly, the return value of this function should be used identically as the `auth` parameter of `http_archive`.
Reference: https://github.com/bazelbuild/bazel/blob/6.3.2/tools/build_defs/repo/http.bzl#L109

The helpers were further modified to support module_ctx.
"""

load("@bazel_tools//tools/build_defs/repo:utils.bzl", "read_netrc", "read_user_netrc", "use_netrc")

# Copied from https://sourcegraph.com/github.com/bazelbuild/bazel@26c6add3f9809611ad3795bce1e5c0fb37902902/-/blob/tools/build_defs/repo/http.bzl
_AUTH_PATTERN_DOC = """An optional dict mapping host names to custom authorization patterns.

If a URL's host name is present in this dict the value will be used as a pattern when
generating the authorization header for the http request. This enables the use of custom
authorization schemes used in a lot of common cloud storage providers.

The pattern currently supports 2 tokens: <code>&lt;login&gt;</code> and
<code>&lt;password&gt;</code>, which are replaced with their equivalent value
in the netrc file for the same host name. After formatting, the result is set
as the value for the <code>Authorization</code> field of the HTTP request.

Example attribute and netrc for a http download to an oauth2 enabled API using a bearer token:

<pre>
auth_patterns = {
    "storage.cloudprovider.com": "Bearer &lt;password&gt;"
}
</pre>

netrc:

<pre>
machine storage.cloudprovider.com
        password RANDOM-TOKEN
</pre>

The final HTTP request would have the following header:

<pre>
Authorization: Bearer RANDOM-TOKEN
</pre>
"""

# AUTH_ATTRS are used within whl_library and pip bzlmod extension.
AUTH_ATTRS = {
    "auth_patterns": attr.string_dict(
        doc = _AUTH_PATTERN_DOC,
    ),
    "netrc": attr.string(
        doc = "Location of the .netrc file to use for authentication",
    ),
}

def get_auth(ctx, urls, ctx_attr = None):
    """Utility for retrieving netrc-based authentication parameters for repository download rules used in python_repository.

    Args:
        ctx(repository_ctx or module_ctx): The extension module_ctx or
            repository rule's repository_ctx object.
        urls: A list of URLs from which assets will be downloaded.
        ctx_attr(struct): The attributes to get the netrc from. When ctx is
            repository_ctx, then we will attempt to use repository_ctx.attr
            if this is not specified, otherwise we will use the specified
            field. The module_ctx attributes are located in the tag classes
            so it cannot be retrieved from the context.

    Returns:
        dict: A map of authentication parameters by URL.
    """

    # module_ctx does not have attributes, as they are stored in tag classes. Whilst
    # the correct behaviour should be to pass the `attr` to the
    ctx_attr = ctx_attr or getattr(ctx, "attr", None)
    ctx_attr = struct(
        netrc = getattr(ctx_attr, "netrc", None),
        auth_patterns = getattr(ctx_attr, "auth_patterns", ""),
    )

    if ctx_attr.netrc:
        netrc = read_netrc(ctx, ctx_attr.netrc)
    elif "NETRC" in ctx.os.environ:
        # This can be used on newer bazel versions
        if hasattr(ctx, "getenv"):
            netrc = read_netrc(ctx, ctx.getenv("NETRC"))
        else:
            netrc = read_netrc(ctx, ctx.os.environ["NETRC"])
    else:
        netrc = read_user_netrc(ctx)

    return use_netrc(netrc, urls, ctx_attr.auth_patterns)
