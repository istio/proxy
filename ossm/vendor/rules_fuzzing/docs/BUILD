# Copyright 2020 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Documentation files for the Bazel fuzzing rules.
# To regenerate, run the 'update_docs.sh' script in the root directory.

# Since the 'bzl_library' rule is currently used only for documentation
# generation, we define all libraries here, to avoid introducing a Skylib
# dependency on our users.
load("@bazel_skylib//:bzl_library.bzl", "bzl_library")
load("@io_bazel_stardoc//stardoc:stardoc.bzl", "stardoc")

bzl_library(
    name = "bazel_skylib",
    deps = [
        "@bazel_skylib//lib:dicts",
        "@bazel_skylib//lib:paths",
        "@bazel_skylib//rules:common_settings",
    ],
)

bzl_library(
    name = "fuzzing_rules",
    srcs = [
        "//fuzzing:cc_defs.bzl",
        "//fuzzing:instrum_opts.bzl",
        "//fuzzing:java_defs.bzl",
        "//fuzzing/private:binary.bzl",
        "//fuzzing/private:common.bzl",
        "//fuzzing/private:engine.bzl",
        "//fuzzing/private:fuzz_test.bzl",
        "//fuzzing/private:instrum_opts.bzl",
        "//fuzzing/private:java_utils.bzl",
        "//fuzzing/private:regression.bzl",
        "//fuzzing/private:util.bzl",
        "//fuzzing/private/oss_fuzz:package.bzl",
        "@rules_cc//cc:bzl_srcs",
        "@rules_fuzzing_oss_fuzz//:instrum.bzl",
        "@rules_java//java:rules",
    ],
    deps = [
        ":bazel_skylib",
    ],
)

stardoc(
    name = "cc_fuzzing_docs",
    out = "cc-fuzzing-rules.md",
    input = "//fuzzing:cc_defs.bzl",
    deps = [
        ":fuzzing_rules",
    ],
)

stardoc(
    name = "java_fuzzing_docs",
    out = "java-fuzzing-rules.md",
    input = "//fuzzing:java_defs.bzl",
    deps = [
        ":fuzzing_rules",
    ],
)
