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

def perl_repositories(bind=True):
    native.git_repository(
        name = "io_bazel_rules_perl",
        remote = "https://github.com/bazelbuild/rules_perl",
        commit = "f6211c2db1e54d0a30bc3c3a718f2b5d45f02a22",
    )

def nginx_test_repositories(bind=True):
    BUILD = """
load("@io_bazel_rules_perl//perl:perl.bzl", "perl_library")

perl_library(
    name = "nginx_test",
    srcs = glob([
        "lib/Test/**/*.pm",
    ]),
    visibility = ["//visibility:public"],
)
"""

    native.new_git_repository(
        name = "nginx_tests_git",
        remote = "https://nginx.googlesource.com/nginx-tests",
        commit = "e740612281f4b64c168e99f2e6d04260d6e9ca28",
        build_file_content = BUILD,
    )

    if bind:
        native.bind(
            name = "nginx_test",
            actual = "@nginx_tests_git//:nginx_test",
        )
