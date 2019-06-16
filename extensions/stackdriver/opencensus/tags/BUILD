# OpenCensus C++ Tags library.
#
# Copyright 2018, OpenCensus Authors
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

load(
    "//extensions/stackdriver/opencensus:copts.bzl",
    "DEFAULT_COPTS",
)

envoy_cc_library(
    name = "tags",
    srcs = [
        "internal/tag_key.cc",
        "internal/tag_map.cc",
    ],
    hdrs = [
        "tag_key.h",
        "tag_map.h",
    ],
    copts = DEFAULT_COPTS,
    repository = "@envoy",
    visibility = ["//extensions/stackdriver:__subpackages__"],
    deps = [
        "//extensions/stackdriver/opencensus/common/internal:hash_mix",
    ],
)

envoy_cc_library(
    name = "context_util",
    srcs = ["internal/context_util.cc"],
    hdrs = ["context_util.h"],
    copts = DEFAULT_COPTS,
    repository = "@envoy",
    visibility = ["//extensions/stackdriver:__subpackages__"],
    deps = [
        ":tags",
        "//extensions/stackdriver/opencensus/context",
    ],
)
