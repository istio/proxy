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

load("@bazel_skylib//rules:common_settings.bzl", "bool_flag", "string_flag")

label_flag(
    name = "cc_engine",
    build_setting_default = "//fuzzing/engines:replay",
    visibility = ["//visibility:public"],
)

label_flag(
    name = "java_engine",
    build_setting_default = "//fuzzing/engines:jazzer",
    visibility = ["//visibility:public"],
)

string_flag(
    name = "cc_engine_instrumentation",
    build_setting_default = "none",
    values = [
        "none",
        "libfuzzer",
        "jazzer",
        "honggfuzz",
        "oss-fuzz",
    ],
    visibility = ["//visibility:public"],
)

string_flag(
    name = "cc_engine_sanitizer",
    build_setting_default = "none",
    values = [
        # No sanitizer instrumentation.
        "none",
        # Address sanitizer (ASAN).
        # See https://clang.llvm.org/docs/AddressSanitizer.html
        "asan",
        # Memory sanitizer (MSAN).
        # See https://clang.llvm.org/docs/MemorySanitizer.html
        "msan",
        # MSAN + origin tracking enabled.
        # Useful for debugging crash reproducers, 1.5-2x slower.
        "msan-origin-tracking",
        # Undefined Behavior sanitizer (UBSAN).
        # See https://clang.llvm.org/docs/UndefinedBehaviorSanitizer.html
        "ubsan",
        # ASAN + UBSAN in a single build.
        "asan-ubsan",
    ],
    visibility = ["//visibility:public"],
)

# If set, define the FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION macro
# during compilation.
#
# See https://llvm.org/docs/LibFuzzer.html#fuzzer-friendly-build-mode
bool_flag(
    name = "cc_fuzzing_build_mode",
    build_setting_default = True,
    visibility = ["//visibility:public"],
)

exports_files([
    "cc_defs.bzl",
    "java_defs.bzl",
    "instrum_opts.bzl",
])
