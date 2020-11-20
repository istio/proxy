# Copyright 2020 Google Inc. All Rights Reserved.
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

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive", "http_file")

def wasm_dependencies():
    FLAT_BUFFERS_SHA = "a83caf5910644ba1c421c002ef68e42f21c15f9f"

    http_archive(
        name = "com_github_google_flatbuffers",
        sha256 = "b8efbc25721e76780752bad775a97c3f77a0250271e2db37fc747b20e8b0f24a",
        strip_prefix = "flatbuffers-" + FLAT_BUFFERS_SHA,
        url = "https://github.com/google/flatbuffers/archive/" + FLAT_BUFFERS_SHA + ".tar.gz",
    )

    http_file(
        name = "com_github_nlohmann_json_single_header",
        sha256 = "3b5d2b8f8282b80557091514d8ab97e27f9574336c804ee666fda673a9b59926",
        urls = [
            "https://github.com/nlohmann/json/releases/download/v3.7.3/json.hpp",
        ],
    )
