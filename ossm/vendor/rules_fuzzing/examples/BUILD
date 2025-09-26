#
# Copyright 2020 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Simple fuzz targets that demonstrate the Bazel extension functionality and
# serve as regression tests. Targets that are expected to crash or hang are
# disabled in the OSS-Fuzz integration using the "no-oss-fuzz" tag.

load("//fuzzing:cc_defs.bzl", "cc_fuzz_test")

cc_fuzz_test(
    name = "empty_fuzz_test",
    size = "small",
    srcs = ["empty_fuzz_test.cc"],
)

filegroup(
    name = "corpus_filegroup",
    srcs = ["corpus_1.txt"] + glob(["corpus_dir/**"]),
)

# This target shows how to create a fuzz test target with corpus files.
cc_fuzz_test(
    name = "empty_fuzz_test_with_corpus",
    timeout = "short",
    srcs = ["empty_fuzz_test.cc"],
    corpus = [
        "corpus_0.txt",
        ":corpus_filegroup",
    ] + glob(["test_corpus_dir/**"]),
)

# This target shows how to create a fuzz test target with dictionaries.
cc_fuzz_test(
    name = "empty_fuzz_test_with_dict",
    srcs = ["empty_fuzz_test.cc"],
    corpus = [
        "corpus_0.txt",
        ":corpus_filegroup",
    ] + glob(["test_corpus_dir/**"]),
    dicts = [
        "dictionaries/valid_part1.dict",
        "dictionaries/valid_part2.dict",
    ],
)

cc_fuzz_test(
    name = "fuzzed_data_provider_fuzz_test",
    srcs = ["fuzzed_data_provider_fuzz_test.cc"],
    target_compatible_with = select({
        "//:clang": [],
        "//conditions:default": ["@platforms//:incompatible"],
    }),
)

cc_fuzz_test(
    name = "hang_fuzz_test",
    srcs = ["hang_fuzz_test.cc"],
    tags = [
        "manual",
        "no-oss-fuzz",
    ],
)

cc_fuzz_test(
    name = "input_buffer_overflow_fuzz_test",
    srcs = ["input_buffer_overflow_fuzz_test.cc"],
    tags = [
        "no-oss-fuzz",
    ],
)

# This test is designed to trigger an uninitialized memory issue.
cc_fuzz_test(
    name = "msan_fuzz_test",
    srcs = ["msan_fuzz_test.cc"],
    copts = ["-Wno-uninitialized"],
    tags = [
        "no-oss-fuzz",
    ],
)

cc_fuzz_test(
    name = "new_buffer_overflow_fuzz_test",
    srcs = ["new_buffer_overflow_fuzz_test.cc"],
    tags = [
        "no-oss-fuzz",
    ],
)

cc_fuzz_test(
    name = "oom_fuzz_test",
    srcs = ["oom_fuzz_test.cc"],
    tags = [
        "manual",
        "no-oss-fuzz",
    ],
)

cc_fuzz_test(
    name = "re2_fuzz_test",
    srcs = ["re2_fuzz_test.cc"],
    deps = [
        "@re2",
    ],
)

cc_fuzz_test(
    name = "runfiles_fuzz_test",
    srcs = ["runfiles_fuzz_test.cc"],
    data = [
        ":corpus_0.txt",
    ],
    deps = [
        "@bazel_tools//tools/cpp/runfiles",
    ],
)

cc_fuzz_test(
    name = "ubsan_int_overflow_fuzz_test",
    srcs = ["ubsan_int_overflow_fuzz_test.cc"],
    tags = [
        "no-oss-fuzz",
    ],
)

cc_fuzz_test(
    name = "ubsan_function_ptr_fuzz_test",
    srcs = ["ubsan_function_ptr_fuzz_test.cc"],
    tags = [
        "no-oss-fuzz",
    ],
)
