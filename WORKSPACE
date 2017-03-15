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

load(
    "//:repositories.bzl",
    "boringssl_repositories",
    "protobuf_repositories",
    "googletest_repositories",
)

boringssl_repositories()

protobuf_repositories()

googletest_repositories()

load(
    "//contrib/endpoints:repositories.bzl",
    "grpc_repositories",
    "servicecontrol_client_repositories",
)

grpc_repositories()

servicecontrol_client_repositories()

load(
    "//src/envoy/mixer:repositories.bzl",
    "mixer_client_repositories",
)

mixer_client_repositories()

# Workaround for Bazel > 0.4.0 since it needs newer protobuf.bzl from:
# https://github.com/google/protobuf/pull/2246
# Do not use this git_repository for anything else than protobuf.bzl
new_git_repository(
    name = "protobuf_bzl",
    # Injecting an empty BUILD file to prevent using any build target
    build_file_content = "",
    commit = "05090726144b6e632c50f47720ff51049bfcbef6",
    remote = "https://github.com/google/protobuf.git",
)

load(
    "@mixerclient_git//:repositories.bzl",
    "mixerapi_repositories",
)

mixerapi_repositories(protobuf_repo="@protobuf_bzl//")

load(
    "//src/envoy:repositories.bzl",
    "envoy_repositories",
)

envoy_repositories()

new_http_archive(
    name = "docker_ubuntu",
    build_file_content = """
load("@bazel_tools//tools/build_defs/docker:docker.bzl", "docker_build")
docker_build(
  name = "xenial",
  tars = ["xenial/ubuntu-xenial-core-cloudimg-amd64-root.tar.gz"],
  visibility = ["//visibility:public"],
)
""",
    sha256 = "de31e6fcb843068965de5945c11a6f86399be5e4208c7299fb7311634fb41943",
    strip_prefix = "docker-brew-ubuntu-core-e406914e5f648003dfe8329b512c30c9ad0d2f9c",
    type = "zip",
    url = "https://codeload.github.com/tianon/docker-brew-ubuntu-core/zip/e406914e5f648003dfe8329b512c30c9ad0d2f9c",
)


DEBUG_BASE_IMAGE_SHA="3f57ae2aceef79e4000fb07ec850bbf4bce811e6f81dc8cfd970e16cdf33e622"

# See github.com/istio/manager/blob/master/docker/debug/build-and-publish-debug-image.sh
# for instructions on how to re-build and publish this base image layer.
http_file(
    name = "ubuntu_xenial_debug",
    url = "https://storage.googleapis.com/istio-build/manager/ubuntu_xenial_debug-" + DEBUG_BASE_IMAGE_SHA + ".tar.gz",
    sha256 = DEBUG_BASE_IMAGE_SHA,
)

# Following go repositories are for building go integration test for mixer filter.
git_repository(
    name = "io_bazel_rules_go",
    commit = "76c63b5cd0d47c1f2b47ab4953db96c574af1c1d", # Jan 16, 2017 (v0.3.3/0.4.0)
    remote = "https://github.com/bazelbuild/rules_go.git",
)

load("@io_bazel_rules_go//go:def.bzl", "go_repositories", "new_go_repository")

go_repositories()

git_repository(
    name = "org_pubref_rules_protobuf",
    commit = "52c843147b50e0f6d7a7d5bb261410e5097f19d3", # Feb 06 2017 (gogo* support)
    remote = "https://github.com/pubref/rules_protobuf",
)

load("@org_pubref_rules_protobuf//protobuf:rules.bzl", "proto_repositories")

proto_repositories()

load("@org_pubref_rules_protobuf//gogo:rules.bzl", "gogo_proto_repositories")
gogo_proto_repositories()

ISTIO_API_BUILD_FILE = """
package(default_visibility = ["//visibility:public"])

load("@io_bazel_rules_go//go:def.bzl", "go_prefix")

go_prefix("istio.io/api")

load("@org_pubref_rules_protobuf//gogo:rules.bzl", "gogoslick_proto_library")

gogoslick_proto_library(
    name = "mixer/v1",
    importmap = {
        "google/rpc/status.proto": "github.com/googleapis/googleapis/google/rpc",
        "google/protobuf/timestamp.proto": "github.com/gogo/protobuf/types",
        "google/protobuf/duration.proto": "github.com/gogo/protobuf/types",
    },
    imports = [
        "../../external/com_github_google_protobuf/src",
        "../../external/com_github_googleapis_googleapis",
    ],
    inputs = [
        "@com_github_google_protobuf//:well_known_protos",
        "@com_github_googleapis_googleapis//:status_proto",
    ],
    protos = [
        "mixer/v1/attributes.proto",
        "mixer/v1/check.proto",
        "mixer/v1/quota.proto",
        "mixer/v1/report.proto",
        "mixer/v1/service.proto",
    ],
    verbose = 0,
    visibility = ["//visibility:public"],
    with_grpc = True,
    deps = [
        "@com_github_gogo_protobuf//sortkeys:go_default_library",
        "@com_github_gogo_protobuf//types:go_default_library",
        "@com_github_googleapis_googleapis//:google/rpc",
    ],
)
"""

new_git_repository(
    name = "com_github_istio_api",
    build_file_content = ISTIO_API_BUILD_FILE,
    commit = "36787dfa214fb9ba24ce2bfaa54d07ab887141f0", # Feb 27, 2017 (no releases)
    remote = "https://github.com/istio/api.git",
)

GOOGLEAPIS_BUILD_FILE = """
package(default_visibility = ["//visibility:public"])

load("@io_bazel_rules_go//go:def.bzl", "go_prefix")
go_prefix("github.com/googleapis/googleapis")

load("@org_pubref_rules_protobuf//gogo:rules.bzl", "gogoslick_proto_library")

gogoslick_proto_library(
    name = "google/rpc",
    protos = [
        "google/rpc/status.proto",
        "google/rpc/code.proto",
    ],
    importmap = {
        "google/protobuf/any.proto": "github.com/gogo/protobuf/types",
    },
    imports = [
        "../../external/com_github_google_protobuf/src",
    ],
    inputs = [
        "@com_github_google_protobuf//:well_known_protos",
    ],
    deps = [
        "@com_github_gogo_protobuf//types:go_default_library",
    ],
    verbose = 0,
)

load("@org_pubref_rules_protobuf//cpp:rules.bzl", "cc_proto_library")

cc_proto_library(
    name = "cc_status_proto",
    protos = [
        "google/rpc/status.proto",
    ],
    imports = [
        "../../external/com_github_google_protobuf/src",
    ],
    verbose = 0,
)

filegroup(
    name = "status_proto",
    srcs = [ "google/rpc/status.proto" ],
)
"""

new_git_repository(
    name = "com_github_googleapis_googleapis",
    build_file_content = GOOGLEAPIS_BUILD_FILE,
    commit = "13ac2436c5e3d568bd0e938f6ed58b77a48aba15", # Oct 21, 2016 (only release pre-dates sha)
    remote = "https://github.com/googleapis/googleapis.git",
)

new_go_repository(
    name = "com_github_google_go_genproto",
    commit = "b3e7c2fb04031add52c4817f53f43757ccbf9c18", # Dec 15, 2016 (no releases)
    importpath = "google.golang.org/genproto",
)

new_go_repository(
    name = "org_golang_google_grpc",
    commit = "708a7f9f3283aa2d4f6132d287d78683babe55c8", # Dec 5, 2016 (v1.0.5)
    importpath = "google.golang.org/grpc",
)
