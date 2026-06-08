# Copyright 2018 The Bazel Authors. All rights reserved.
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

"""Dependencies for Rust proto rules"""

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")
load("//3rdparty/crates:crates.bzl", "crate_repositories")

def rust_proto_protobuf_dependencies(bzlmod = False):
    """Sets up dependencies for rules_rust's proto support.

    Args:
        bzlmod (bool): Whether this function is being called from a bzlmod context rather than a workspace context.

    Returns:
        A list of structs containing information about root module deps to report to bzlmod's extension_metadata.

    """
    if not bzlmod:
        maybe(
            http_archive,
            name = "rules_proto",
            sha256 = "6fb6767d1bef535310547e03247f7518b03487740c11b6c6adb7952033fe1295",
            strip_prefix = "rules_proto-6.0.2",
            url = "https://github.com/bazelbuild/rules_proto/releases/download/6.0.2/rules_proto-6.0.2.tar.gz",
        )

        maybe(
            http_archive,
            name = "com_google_protobuf",
            integrity = "sha256-fD69eq7dhvpdxHmg/agD9gLKr3jYr/fOg7ieG4rnRCo=",
            strip_prefix = "protobuf-28.3",
            urls = ["https://github.com/protocolbuffers/protobuf/releases/download/v28.3/protobuf-28.3.tar.gz"],
        )

    return crate_repositories()

# buildifier: disable=unnamed-macro
def rust_proto_protobuf_register_toolchains(register_toolchains = True):
    """Register toolchains for proto compilation."""

    if register_toolchains:
        native.register_toolchains(str(Label("//:default_proto_toolchain")))
