# Copyright 2022 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

load("@rules_fuzzing//fuzzing:cc_defs.bzl", "cc_fuzz_test")

licenses(["notice"])  # Apache 2

package(default_visibility = ["//visibility:public"])

filegroup(
    name = "corpus_bytecode",
    srcs = glob(["corpus_bytecode/**"]),
)

cc_fuzz_test(
    name = "bytecode_util_fuzzer",
    srcs = ["bytecode_util_fuzzer.cc"],
    corpus = [":corpus_bytecode"],
    deps = [
        "//:lib",
    ],
)

filegroup(
    name = "corpus_pairs",
    srcs = glob(["corpus_pairs/**"]),
)

cc_fuzz_test(
    name = "pairs_util_fuzzer",
    srcs = ["pairs_util_fuzzer.cc"],
    corpus = [":corpus_pairs"],
    deps = [
        "//:lib",
    ],
)
