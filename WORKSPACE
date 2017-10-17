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
local_repository(
    name = "mixerclient_git",
    path = "vendor/mixerclient",
)

bind(
    name = "mixer_client_lib",
    actual = "@mixerclient_git//:mixer_client_lib",
)


new_local_repository(
    name = "gogoproto_git",
    path = "vendor/gogoproto",
    build_file = "tools/gogo.BUILD",
)

bind(
    name = "cc_gogoproto",
    actual = "@gogoproto_git//:cc_gogoproto",
)

bind(
    name = "cc_gogoproto_genproto",
    actual = "@gogoproto_git//:cc_gogoproto_genproto",
)

# TODO: check in the BUILD file, part of the proxy BUILD
new_local_repository(
    name = "mixerapi_git",
    path = "vendor/mixerapi",
    build_file = "tools/mixerapi.BUILD",
)

bind(
    name = "mixer_api_cc_proto",
    actual = "@mixerapi_git//:mixer_api_cc_proto",
)

load(
    "@mixerclient_git//:repositories.bzl",
    "googleapis_repositories",
)

googleapis_repositories()


bind(
    name = "boringssl_crypto",
    actual = "//external:ssl",
)

local_repository(
    name = "envoy",
    path = "envoy",
)

# TODO: replace with local_repository to use those picked by repo
load("@envoy//bazel:repositories.bzl", "envoy_dependencies")

envoy_dependencies()

load("@envoy//bazel:cc_configure.bzl", "cc_configure")

cc_configure()

load("@envoy_api//bazel:repositories.bzl", "api_dependencies")

api_dependencies()

# Following go repositories are for building go integration test for mixer filter.
git_repository(
    name = "io_bazel_rules_go",
    commit = "9cf23e2aab101f86e4f51d8c5e0f14c012c2161c",  # Oct 12, 2017 (Add `build_external` option to `go_repository`)
    remote = "https://github.com/bazelbuild/rules_go.git",
)

load("@io_bazel_rules_go//go:def.bzl", "go_rules_dependencies", "go_register_toolchains")
go_rules_dependencies()
go_register_toolchains()

load("@io_bazel_rules_go//proto:def.bzl", "proto_register_toolchains")
proto_register_toolchains()

local_repository(
    name = "com_github_istio_mixer",
    path = "go/src/istio.io/mixer"
)

load("@com_github_istio_mixer//test:repositories.bzl", "mixer_test_repositories")

mixer_test_repositories()

