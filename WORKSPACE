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
# Determine SHA256 `wget https://github.com/envoyproxy/envoy-wasm/archive/COMMIT.tar.gz && sha256sum COMMIT.tar.gz`
# envoy-wasm commit date  06/12/2019
# bazel version: 0.25.0
ENVOY_SHA = "72ac3212de661d2ef7c9210e78fedf76d5cbe9f0"

ENVOY_SHA256 = "cdf7e17398f8767b9fe63538129ac8b5231e7a017cd478a4642205575522cd47"

LOCAL_ENVOY_PROJECT = "/PATH/TO/ENVOY"

http_archive(
    name = "envoy",
    sha256 = ENVOY_SHA256,
    strip_prefix = "envoy-wasm-" + ENVOY_SHA,
    url = "https://github.com/envoyproxy/envoy-wasm/archive/" + ENVOY_SHA + ".tar.gz",
)

# TODO(silentdai) Use bazel args to select envoy between local or http
# Uncomment below and comment above http_archive to depends on local envoy.
#local_repository(
#     name = "envoy",
#     path = LOCAL_ENVOY_PROJECT,
#)

load("@envoy//bazel:api_repositories.bzl", "envoy_api_dependencies")

envoy_api_dependencies()

load("@envoy//bazel:repositories.bzl", "GO_VERSION", "envoy_dependencies")

envoy_dependencies()

load("@rules_foreign_cc//:workspace_definitions.bzl", "rules_foreign_cc_dependencies")

rules_foreign_cc_dependencies()

load("@envoy//bazel:cc_configure.bzl", "cc_configure")

cc_configure()

load("@envoy_api//bazel:repositories.bzl", "api_dependencies")

api_dependencies()

load("@io_bazel_rules_go//go:deps.bzl", "go_register_toolchains", "go_rules_dependencies")

go_rules_dependencies()

go_register_toolchains(go_version = GO_VERSION)

# TODO(bianpengyuan): remove this googleapis dep when upstream imports it in
# https://github.com/envoyproxy/envoy/pull/5387
load("//extensions/stackdriver/opencensus:opencensus.bzl", "telemetry_googleapis")

telemetry_googleapis()

load("@telemetry_googleapis//:repository_rules.bzl", "switched_rules_by_language")

switched_rules_by_language(
    name = "com_google_googleapis_imports",
    cc = True,
)
