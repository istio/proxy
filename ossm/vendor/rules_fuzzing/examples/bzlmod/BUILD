# Copyright 2024 Google LLC
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

load("@my_rules_fuzzing//fuzzing:cc_defs.bzl", "cc_fuzz_test")
load("@my_rules_fuzzing//fuzzing:java_defs.bzl", "java_fuzz_test")

cc_fuzz_test(
    name = "cc_fuzz_test",
    srcs = ["cc_fuzz_test.cc"],
)

java_fuzz_test(
    name = "java_fuzz_test",
    srcs = ["JavaFuzzTest.java"],
    target_class = "com.example.JavaFuzzTest",
)
