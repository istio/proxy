# OpenCensus C++ Stats library.
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

envoy_cc_library(
    name = "stats",
    hdrs = [
        "internal/aggregation_window.h",
        "internal/set_aggregation_window.h",
        "stats.h",
        "stats_exporter.h",
    ],
    copts = [
        "-DNULL_PLUGIN=1",
        "-Iextensions/stackdriver",
    ],
    repository = "@envoy",
    visibility = ["//extensions/stackdriver:__subpackages__"],
    deps = [
        ":core",
        ":recording",
    ],
)

envoy_cc_library(
    name = "core",
    srcs = [
        "internal/aggregation.cc",
        "internal/aggregation_window.cc",
        "internal/bucket_boundaries.cc",
        "internal/delta_producer.cc",
        "internal/distribution.cc",
        "internal/measure.cc",
        "internal/measure_data.cc",
        "internal/measure_descriptor.cc",
        "internal/measure_registry.cc",
        "internal/measure_registry_impl.cc",
        "internal/set_aggregation_window.cc",
        "internal/stats_exporter.cc",
        "internal/stats_manager.cc",
        "internal/view.cc",
        "internal/view_data.cc",
        "internal/view_data_impl.cc",
        "internal/view_descriptor.cc",
    ],
    hdrs = [
        "aggregation.h",
        "bucket_boundaries.h",
        "distribution.h",
        "internal/aggregation_window.h",
        "internal/delta_producer.h",
        "internal/measure_data.h",
        "internal/measure_registry_impl.h",
        "internal/set_aggregation_window.h",
        "internal/stats_exporter_impl.h",
        "internal/stats_manager.h",
        "internal/view_data_impl.h",
        "measure.h",
        "measure_descriptor.h",
        "measure_registry.h",
        "stats_exporter.h",
        "tag_key.h",
        "tag_set.h",
        "view.h",
        "view_data.h",
        "view_descriptor.h",
    ],
    copts = [
        "-DNULL_PLUGIN=1",
        "-Iextensions/stackdriver",
    ],
    repository = "@envoy",
    deps = [
        "//extensions/stackdriver/opencensus/common/internal:string_vector_hash",
        "//extensions/stackdriver/opencensus/tags",
        "@envoy//source/extensions/common/wasm/null:null_lib",
    ],
)

envoy_cc_library(
    name = "recording",
    srcs = ["internal/recording.cc"],
    hdrs = ["recording.h"],
    copts = [
        "-DNULL_PLUGIN=1",
        "-Iextensions/stackdriver",
    ],
    repository = "@envoy",
    deps = [
        ":core",
        "//extensions/stackdriver/opencensus/tags",
        "//extensions/stackdriver/opencensus/tags:context_util",
    ],
)
