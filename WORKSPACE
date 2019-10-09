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

# 1. Determine SHA256 `wget https://github.com/envoyproxy/envoy-wasm/archive/$COMMIT.tar.gz && sha256sum $COMMIT.tar.gz`
# 2. Update .bazelrc and .bazelversion files.
#
# envoy-wasm commit date: 10/09/2019
ENVOY_SHA = "ec03328688895d99c9f5ae4fd7f8459ef3e95212"

ENVOY_SHA256 = "4abf05fd040b56af630b624068d462035bd986bccf775f821b755fd3c0df8083"

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

load("@envoy//bazel:api_binding.bzl", "envoy_api_binding")

envoy_api_binding()

load("@envoy//bazel:api_repositories.bzl", "envoy_api_dependencies")

envoy_api_dependencies()

load("@envoy//bazel:repositories.bzl", "envoy_dependencies")

envoy_dependencies()

load("@envoy//bazel:dependency_imports.bzl", "envoy_dependency_imports")

envoy_dependency_imports()
