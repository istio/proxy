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

load("@rules_fuzzing//fuzzing:cc_defs.bzl", "cc_fuzzing_engine")
load("@rules_fuzzing//fuzzing:java_defs.bzl", "java_fuzzing_engine")

cc_fuzzing_engine(
    name = "oss_fuzz_engine",
    display_name = "OSS-Fuzz",
    launcher = "oss_fuzz_launcher.sh",
    library = ":oss_fuzz_stub",
    visibility = ["//visibility:public"],
)

cc_library(
    name = "oss_fuzz_stub",
    srcs = [%{stub_srcs}],
    linkopts = [%{stub_linkopts}],
)

java_fuzzing_engine(
    name = "oss_fuzz_java_engine",
    display_name = "OSS-Fuzz (Java)",
    launcher = "oss_fuzz_launcher.sh",
    library = ":oss_fuzz_java_stub",
    visibility = ["//visibility:public"],
)

java_import(
    name = "oss_fuzz_java_stub",
    jars = [%{jazzer_jars}],
)


exports_files(["instrum.bzl"])
