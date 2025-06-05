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
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository")

def absl_repositories(bind = True):
    http_archive(
        name = "com_google_absl",
        sha256 = "ea1d31db00eb37e607bfda17ffac09064670ddf05da067944c4766f517876390",
        strip_prefix = "abseil-cpp-c2435f8342c2d0ed8101cb43adfd605fdc52dca2",  # May 04, 2023.
        urls = ["https://github.com/abseil/abseil-cpp/archive/c2435f8342c2d0ed8101cb43adfd605fdc52dca2.zip"],
    )

PROTOBUF_COMMIT = "315ffb5be89460f2857387d20aefc59b76b8bdc3"  # May 31, 2023
PROTOBUF_SHA256 = "aa61db6ff113a1c76eac9408144c6e996c5e2d6b2410818fd7f1b0d222a50bf8"

def protobuf_repositories(bind = True):
    http_archive(
        name = "com_google_protobuf",
        strip_prefix = "protobuf-" + PROTOBUF_COMMIT,
        urls = [
            "https://github.com/google/protobuf/archive/" + PROTOBUF_COMMIT + ".tar.gz",
        ],
        sha256 = PROTOBUF_SHA256,
    )

GOOGLETEST_COMMIT = "f8d7d77c06936315286eb55f8de22cd23c188571"  # v1.14.0: Aug 2, 2023
GOOGLETEST_SHA256 = "7ff5db23de232a39cbb5c9f5143c355885e30ac596161a6b9fc50c4538bfbf01"

def googletest_repositories(bind = True):
    http_archive(
        name = "com_google_googletest",
        strip_prefix = "googletest-" + GOOGLETEST_COMMIT,
        url = "https://github.com/google/googletest/archive/" + GOOGLETEST_COMMIT + ".tar.gz",
        sha256 = GOOGLETEST_SHA256,
    )

GOOGLEAPIS_COMMIT = "1d5522ad1056f16a6d593b8f3038d831e64daeea"  # Sept 03, 2020
GOOGLEAPIS_SHA256 = "cd13e547cffaad217c942084fd5ae0985a293d0cce3e788c20796e5e2ea54758"

def googleapis_repositories(bind = True):
    http_archive(
        name = "com_google_googleapis",
        strip_prefix = "googleapis-" + GOOGLEAPIS_COMMIT,
        url = "https://github.com/googleapis/googleapis/archive/" + GOOGLEAPIS_COMMIT + ".tar.gz",
        sha256 = GOOGLEAPIS_SHA256,
    )

GOOGLEBENCHMARK_COMMIT = "1.7.0"  # Jul 25, 2022
GOOGLEBENCHMARK_SHA256 = "3aff99169fa8bdee356eaa1f691e835a6e57b1efeadb8a0f9f228531158246ac"

def googlebenchmark_repositories(bind = True):
    http_archive(
        name = "com_google_benchmark",
        strip_prefix = "benchmark-" + GOOGLEBENCHMARK_COMMIT,
        url = "https://github.com/google/benchmark/archive/v" + GOOGLEBENCHMARK_COMMIT + ".tar.gz",
        sha256 = GOOGLEBENCHMARK_SHA256,
    )

def nlohmannjson_repositories(bind = True):
    http_archive(
        name = "com_github_nlohmann_json",
        strip_prefix = "json-3.11.3",
        urls = ["https://github.com/nlohmann/json/archive/v3.11.3.tar.gz"],
        sha256 = "0d8ef5af7f9794e3263480193c491549b2ba6cc74bb018906202ada498a79406",
    )

RULES_DOCKER_COMMIT = "0.25.0"  # Jun 22, 2022
RULES_DOCKER_SHA256 = "b1e80761a8a8243d03ebca8845e9cc1ba6c82ce7c5179ce2b295cd36f7e394bf"

def io_bazel_rules_docker(bind = True):
    http_archive(
        name = "io_bazel_rules_docker",
        sha256 = RULES_DOCKER_SHA256,
        urls = ["https://github.com/bazelbuild/rules_docker/releases/download/v" + RULES_DOCKER_COMMIT + "/rules_docker-v" + RULES_DOCKER_COMMIT + ".tar.gz"],
    )

def protoconverter_repositories(bind = True):
    http_archive(
        name = "com_google_protoconverter",
        sha256 = "6081836fa3838ebb1aa15089a5c3e20f877a0244c7a39b92a2000efb40408dcb",
        strip_prefix = "proto-converter-d77ff301f48bf2e7a0f8935315e847c1a8e00017",
        urls = ["https://github.com/grpc-ecosystem/proto-converter/archive/d77ff301f48bf2e7a0f8935315e847c1a8e00017.zip"],
    )
