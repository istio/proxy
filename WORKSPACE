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
load(
    "//src/envoy/mixer/integration_test:repositories.bzl",
    "integration_test_repositories",
)
integration_test_repositories()

load(
    "//src/envoy/mixer/integration_test:mmm.bzl",
    "mmm",
)
mmm()

load("@io_bazel_rules_go//go:def.bzl", "go_repositories", "go_repository", "new_go_repository")
#go_repositories()

load("@org_pubref_rules_protobuf//protobuf:rules.bzl", "proto_repositories")
proto_repositories()

load("@org_pubref_rules_protobuf//gogo:rules.bzl", "gogo_proto_repositories")
gogo_proto_repositories()

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

new_go_repository(
    name = "com_github_golang_glog",
    commit = "23def4e6c14b4da8ac2ed8007337bc5eb5007998", # Jan 26, 2016 (no releases)
    importpath = "github.com/golang/glog",
)

new_go_repository(
    name = "com_github_ghodss_yaml",
    commit = "04f313413ffd65ce25f2541bfd2b2ceec5c0908c", # Dec 6, 2016 (no releases)
    importpath = "github.com/ghodss/yaml",
)

new_go_repository(
    name = "in_gopkg_yaml_v2",
    commit = "14227de293ca979cf205cd88769fe71ed96a97e2", # Jan 24, 2017 (no releases)
    importpath = "gopkg.in/yaml.v2",
)

new_go_repository(
    name = "com_github_spf13_cobra",
    commit = "35136c09d8da66b901337c6e86fd8e88a1a255bd", # Jan 30, 2017 (no releases)
    importpath = "github.com/spf13/cobra",
)

new_go_repository(
    name = "com_github_spf13_pflag",
    commit = "9ff6c6923cfffbcd502984b8e0c80539a94968b7", # Jan 30, 2017 (no releases)
    importpath = "github.com/spf13/pflag",
)

new_go_repository(
    name = "com_github_hashicorp_go_multierror",
    commit = "ed905158d87462226a13fe39ddf685ea65f1c11f", # Dec 16, 2016 (no releases)
    importpath = "github.com/hashicorp/go-multierror",
)

new_go_repository(
    name = "com_github_hashicorp_errwrap",
    commit = "7554cd9344cec97297fa6649b055a8c98c2a1e55", # Oct 27, 2014 (no releases)
    importpath = "github.com/hashicorp/errwrap",
)

new_go_repository(
    name = "com_github_opentracing_opentracing_go",
    commit = "0c3154a3c2ce79d3271985848659870599dfb77c", # Sep 26, 2016 (v1.0.0)
    importpath = "github.com/opentracing/opentracing-go",
)

new_go_repository(
    name = "com_github_opentracing_basictracer",
    commit = "1b32af207119a14b1b231d451df3ed04a72efebf", # Sep 29, 2016 (no releases)
    importpath = "github.com/opentracing/basictracer-go",
)

go_repository(
    name = "com_github_istio_mixer",
    commit = "0b2ef133ff6c912855cd059a8f98721ed51d0f93",
    importpath = "github.com/istio/mixer",
)
