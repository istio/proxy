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

load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("@rules_java//java:java_library.bzl", "java_library")
load("//fuzzing:cc_defs.bzl", "cc_fuzzing_engine")
load("//fuzzing:java_defs.bzl", "java_fuzzing_engine")

# libFuzzer specification.
##########################

cc_fuzzing_engine(
    name = "libfuzzer",
    display_name = "libFuzzer",
    launcher = "libfuzzer_launcher.sh",
    library = ":libfuzzer_stub",
    visibility = ["//visibility:public"],
)

cc_library(
    name = "libfuzzer_stub",
    linkopts = [
        # We add the linker options to the library, so only the fuzz test binary
        # is linked with libfuzzer.
        "-fsanitize=fuzzer",
    ],
)

# Honggfuzz specification.
##########################

cc_fuzzing_engine(
    name = "honggfuzz",
    display_name = "Honggfuzz",
    launcher = "honggfuzz_launcher.sh",
    launcher_data = {
        "@honggfuzz//:honggfuzz": "HONGGFUZZ_PATH",
    },
    library = "@honggfuzz//:honggfuzz_engine",
    visibility = ["//visibility:public"],
)

# The replay engine specification.
##################################

cc_fuzzing_engine(
    name = "replay",
    display_name = "Replay",
    launcher = "replay_launcher.sh",
    library = "//fuzzing/replay:replay_main",
    visibility = ["//visibility:public"],
)

# Jazzer specification.
#################################

java_fuzzing_engine(
    name = "jazzer",
    display_name = "Jazzer",
    launcher = "jazzer_launcher.sh",
    library = ":jazzer_stub",
    visibility = ["//visibility:public"],
)

# This wrapper target is needed as Jazzer consists of two separate Java targets,
# but java_fuzzing_engine's library attribute only accepts a single target.
java_library(
    name = "jazzer_stub",
    exports = [
        "@rules_fuzzing_jazzer//jar",
        "@rules_fuzzing_jazzer_api//jar",
    ],
)

# OSS-Fuzz specification.
#########################

alias(
    name = "oss_fuzz",
    actual = "@rules_fuzzing_oss_fuzz//:oss_fuzz_engine",
    visibility = ["//visibility:public"],
)

alias(
    name = "oss_fuzz_java",
    actual = "@rules_fuzzing_oss_fuzz//:oss_fuzz_java_engine",
    visibility = ["//visibility:public"],
)
