#  Copyright 2020 Google LLC
#
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")

def proxy_wasm_cpp_sdk_repositories():
    maybe(
        http_archive,
        name = "emsdk",
        sha256 = "0cb0eabd6e3ceb1a970a2363e67f2b1689c2d83fbeae1e75901213c1f84de2e2",
        strip_prefix = "emsdk-3.1.67/bazel",
        url = "https://github.com/emscripten-core/emsdk/archive/refs/tags/3.1.67.tar.gz",
    )

    # protobuf depends on rules_python py_repositories
    maybe(
        http_archive,
        name = "rules_python",
        sha256 = "778aaeab3e6cfd56d681c89f5c10d7ad6bf8d2f1a72de9de55b23081b2d31618",
        strip_prefix = "rules_python-0.34.0",
        url = "https://github.com/bazelbuild/rules_python/releases/download/0.34.0/rules_python-0.34.0.tar.gz",
    )

    maybe(
        http_archive,
        name = "com_google_protobuf",
        sha256 = "4fc5ff1b2c339fb86cd3a25f0b5311478ab081e65ad258c6789359cd84d421f8",
        strip_prefix = "protobuf-26.1",
        url = "https://github.com/protocolbuffers/protobuf/releases/download/v26.1/protobuf-26.1.tar.gz",
    )

    maybe(
        http_archive,
        name = "com_google_absl",
        sha256 = "95e90be7c3643e658670e0dd3c1b27092349c34b632c6e795686355f67eca89f",
        strip_prefix = "abseil-cpp-20240722.0",
        urls = ["https://github.com/abseil/abseil-cpp/archive/20240722.0.zip"],
    )

    maybe(
        http_archive,
        name = "com_google_re2",
        sha256 = "18cf85922e27fad3ed9c96a27733037da445f35eb1a2744c306a37c6d11e95c4",
        strip_prefix = "re2-2023-07-01",
        url = "https://github.com/google/re2/archive/2023-07-01.tar.gz",
    )
