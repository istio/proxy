# Copyright 2016 Google Inc. All Rights Reserved.
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

# http_archive is not a native function since bazel 0.19
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

load(
     "//:repositories.bzl",
     "googletest_repositories",
     "mixerapi_dependencies",
)

googletest_repositories()
mixerapi_dependencies()

bind(
    name = "boringssl_crypto",
    actual = "//external:ssl",
)

# When updating envoy sha manually please update the sha in istio.deps file also
#
# Determine SHA256 `wget https://github.com/envoyproxy/envoy/archive/COMMIT.tar.gz && sha256sum COMMIT.tar.gz`
ENVOY_SHA = "805683f835bd63e4b7b9d89059aa0d3783924a93"
ENVOY_SHA256 = "75a029fb3904c17f47c7f723e2a04468bfc007bb4cfc74fe21f82cf799d8a904"

http_archive(
    name = "envoy",
    strip_prefix = "envoy-" + ENVOY_SHA,
    url = "https://github.com/envoyproxy/envoy/archive/" + ENVOY_SHA + ".tar.gz",
    sha256 = ENVOY_SHA256,
)

load("@envoy//bazel:repositories.bzl", "envoy_dependencies")
envoy_dependencies()

load("@rules_foreign_cc//:workspace_definitions.bzl", "rules_foreign_cc_dependencies")
rules_foreign_cc_dependencies()

load("@envoy//bazel:cc_configure.bzl", "cc_configure")
cc_configure()

load("@envoy_api//bazel:repositories.bzl", "api_dependencies")
api_dependencies()

load("@io_bazel_rules_go//go:deps.bzl", "go_rules_dependencies", "go_register_toolchains")
go_rules_dependencies()
go_register_toolchains()

# Nov 28, 2017 (bazel 0.8.0 support)
RULES_PROTOBUF_SHA = "563b674a2ce6650d459732932ea2bc98c9c9a9bf"
RULES_PROTOBUF_SHA256 = "338e0d65cd709c6a6f9b5702466e641d536479be8b564d1e12a5d1de22a5cff6"

http_archive(
    name = "org_pubref_rules_protobuf",
    strip_prefix = "rules_protobuf-" + RULES_PROTOBUF_SHA,
    url = "https://github.com/pubref/rules_protobuf/archive/" + RULES_PROTOBUF_SHA + ".tar.gz",
    sha256 = RULES_PROTOBUF_SHA256,
)
