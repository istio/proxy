# Copyright 2019 Istio Authors. All Rights Reserved.
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

def io_opencensus_cpp():
    # commit date: Jun 5, 2019
    OPENCENSUS_CPP_SHA = "a506cf846edca75b93e5457aca51c568378201be"
    OPENCENSUS_CPP_SHA256 = "3bffa3b48b415f94b1a7bb36f7a0ccebdf29cd2281731864a90f80be50be776c"
    OPENCENSUS_CPP_URL = "https://github.com/census-instrumentation/opencensus-cpp/archive/" + OPENCENSUS_CPP_SHA + ".tar.gz"

    http_archive(
        name = "io_opencensus_cpp",
        url = OPENCENSUS_CPP_URL,
        patch_args = ["-p1"],
        patches = ["//extensions/stackdriver:opencensus/io_opencensus_cpp_null.patch"],
        sha256 = OPENCENSUS_CPP_SHA256,
        strip_prefix = "opencensus-cpp-" + OPENCENSUS_CPP_SHA,
    )
