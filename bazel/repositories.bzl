# Copyright Istio Authors. All Rights Reserved.
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
#

def _define_envoy_implementation_impl(ctx):
    if ctx.os.environ.get("ENVOY_OPENSSL") == "1":
        attr = ctx.attr.openssl
        openssl_disabled_extensions = '''
            "envoy.tls.key_providers.cryptomb",
            "envoy.tls.key_providers.qat",
            "envoy.quic.deterministic_connection_id_generator",
            "envoy.quic.crypto_stream.server.quiche",
            "envoy.quic.proof_source.filter_chain",
        '''
    else:
        attr = ctx.attr.boringssl
        openssl_disabled_extensions = ''

    ctx.file("BUILD", "")
    ctx.file("load_envoy.bzl", '''
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

OPENSSL_DISABLED_EXTENSIONS = [%s]

def load_envoy():
    http_archive(
        name = "envoy",
        sha256 = "%s",
        strip_prefix = "%s-%s",
        url = "https://github.com/%s/%s/archive/%s.tar.gz",
    )
''' % (openssl_disabled_extensions, attr["sha256"], attr["repo"], attr["sha"], attr["org"], attr["repo"], attr["sha"])
    )

define_envoy_implementation = repository_rule(
    implementation = _define_envoy_implementation_impl,
    attrs = {
        "boringssl": attr.string_dict(allow_empty = False, mandatory = True),
        "openssl": attr.string_dict(allow_empty = False, mandatory = True),
    },
    environ = ["ENVOY_OPENSSL"],
)
