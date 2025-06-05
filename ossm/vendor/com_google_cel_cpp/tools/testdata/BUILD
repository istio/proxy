# Copyright 2021 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

load(
    "@com_github_google_flatbuffers//:build_defs.bzl",
    "flatbuffer_library_public",
)

licenses(["notice"])

package(default_visibility = ["//visibility:public"])

flatbuffer_library_public(
    name = "flatbuffers_test",
    srcs = ["flatbuffers.fbs"],
    outs = ["flatbuffers_generated.h"],
    language_flag = "-c",
    reflection_name = "flatbuffers_reflection",
)

filegroup(
    name = "coverage_testdata",
    srcs = [
        "coverage_example.textproto",
        "exists_macro.textproto",
    ],
)

cc_library(
    name = "flatbuffers_test_cc",
    srcs = [":flatbuffers_test"],
    hdrs = [":flatbuffers_test"],
    features = ["-parse_headers"],
    linkstatic = True,
    deps = ["@com_github_google_flatbuffers//:runtime_cc"],
)
