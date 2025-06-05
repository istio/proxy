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

# Tools for replaying test files.

load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("@rules_cc//cc:cc_test.bzl", "cc_test")

package(default_visibility = ["//fuzzing:__subpackages__"])

# Libraries.
############

cc_library(
    name = "file_util",
    srcs = ["file_util.cc"],
    hdrs = ["file_util.h"],
    deps = [
        ":status_util",
        "@com_google_absl//absl/functional:function_ref",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
    ],
)

WEAK_LLVM_FUZZER_INITIALIZE = select({
    "@platforms//os:osx": ["-Wl,-U,_LLVMFuzzerInitialize"],
    "//conditions:default": [],
})

cc_library(
    name = "replay_main",
    srcs = ["replay_main.cc"],
    linkopts = WEAK_LLVM_FUZZER_INITIALIZE,
    deps = [
        ":test_replayer",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
    ],
    alwayslink = True,
)

cc_library(
    name = "status_util",
    srcs = ["status_util.cc"],
    hdrs = ["status_util.h"],
    deps = [
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "test_file_buffer",
    srcs = ["test_file_buffer.cc"],
    hdrs = ["test_file_buffer.h"],
    deps = [
        ":status_util",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "test_replayer",
    srcs = ["test_replayer.cc"],
    hdrs = ["test_replayer.h"],
    deps = [
        ":file_util",
        ":test_file_buffer",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
    ],
)

# Tests.
########

cc_test(
    name = "file_util_test",
    size = "small",
    srcs = ["file_util_test.cc"],
    deps = [
        ":file_util",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "status_util_test",
    size = "small",
    srcs = ["status_util_test.cc"],
    deps = [
        ":status_util",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "test_file_buffer_test",
    size = "small",
    srcs = ["test_file_buffer_test.cc"],
    deps = [
        ":file_util",
        ":test_file_buffer",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "test_replayer_test",
    size = "small",
    srcs = ["test_replayer_test.cc"],
    deps = [
        ":file_util",
        ":test_replayer",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)
