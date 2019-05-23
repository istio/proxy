# OpenCensus C++ internal libraries.
#
# Copyright 2017, OpenCensus Authors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
)

licenses(["notice"])  # Apache 2.0

envoy_cc_library(
    name = "hash_mix",
    hdrs = ["hash_mix.h"],
    copts = [
        "-DNULL_PLUGIN=1",
        "-Iextensions/stackdriver",
    ],
    repository = "@envoy",
    visibility = ["//extensions/stackdriver:__subpackages__"],
)

envoy_cc_library(
    name = "random_lib",
    srcs = ["random.cc"],
    hdrs = ["random.h"],
    copts = [
        "-DNULL_PLUGIN=1",
        "-Iextensions/stackdriver",
    ],
    repository = "@envoy",
    visibility = ["//extensions/stackdriver:__subpackages__"],
    deps = [
        "@envoy//source/extensions/common/wasm/null:null_lib",
    ],
)

envoy_cc_library(
    name = "string_vector_hash",
    hdrs = ["string_vector_hash.h"],
    copts = [
        "-DNULL_PLUGIN=1",
        "-Iextensions/stackdriver",
    ],
    repository = "@envoy",
    visibility = ["//extensions/stackdriver:__subpackages__"],
    deps = [":hash_mix"],
)
