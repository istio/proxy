# Copyright 2019 Istio Authors. All Rights Reserved.
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
#
################################################################################

def _envoy_repository_rule_impl(ctx):
    """Core implementation of envoy_repository_rule."""

    # Set repo, sha, sha256, and prefix from environment (if defined) or supplied attribute.
    _repo = ctx.os.environ.get("ENVOY_REPOSITORY", default = ctx.attr.repository)
    _sha = ctx.os.environ.get("ENVOY_SHA", default = ctx.attr.sha)
    _sha256 = ctx.os.environ.get("ENVOY_SHA256", default = ctx.attr.sha256)
    _prefix = ctx.os.environ.get("ENVOY_PREFIX", default = ctx.attr.prefix)

    # Download and extract archive.
    ctx.download_and_extract(
        sha256 = _sha256,
        stripPrefix = _prefix + _sha,
        url = _repo + "/archive/" + _sha + ".tar.gz",
    )

envoy_repository_rule = repository_rule(
    environ = ["ENVOY_REPOSITORY", "ENVOY_SHA", "ENVOY_SHA256", "ENVOY_PREFIX"],
    implementation = _envoy_repository_rule_impl,
    attrs = {
        "repository": attr.string(mandatory = True),
        "sha": attr.string(mandatory = True),
        "sha256": attr.string(mandatory = True),
        "prefix": attr.string(mandatory = True),
    },
)
