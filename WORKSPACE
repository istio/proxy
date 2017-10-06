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
    commit = "7991b6353e468ba5e8403af382241d9ce031e571",  # Aug 1, 2017 (gazelle fixes)
    remote = "https://github.com/bazelbuild/rules_go.git",
)

git_repository(
    name = "org_pubref_rules_protobuf",
    commit = "9ede1dbc38f0b89ae6cd8e206a22dd93cc1d5637",
    remote = "https://github.com/pubref/rules_protobuf",
)

MIXER = "535eb564667cef6aed334cb4f5e967a104768387"

git_repository(
    name = "com_github_istio_mixer",
    commit = MIXER,
    remote = "https://github.com/istio/mixer",
)

load("@com_github_istio_mixer//test:repositories.bzl", "mixer_test_repositories")

mixer_test_repositories()
