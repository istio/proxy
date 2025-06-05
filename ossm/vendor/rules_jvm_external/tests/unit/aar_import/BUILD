# Copyright 2021 The Bazel Authors. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

load("@bazel_skylib//rules:build_test.bzl", "build_test")
load("@rules_android//android:rules.bzl", "aar_import")
load(":aar_import_test.bzl", "aar_import_test_suite")

aar_import(
    name = "aar_import_that_consumes_the_downloaded_file_directly",
    # Will produce an error if the downloaded file does not have the `.aar` file extension
    aar = "@com_android_support_appcompat_v7_aar_28_0_0//file:file",
)

build_test(
    name = "test_does_aar_artifact_have_the_correct_file_extension",
    targets = [
        ":aar_import_that_consumes_the_downloaded_file_directly",
    ],
)

build_test(
    name = "starlark_aar_import_test",
    targets = [
        "@starlark_aar_import_test//:com_android_support_appcompat_v7_28_0_0",
    ],
)

build_test(
    name = "starlark_aar_import_with_sources_test",
    targets = [
        "@starlark_aar_import_with_sources_test//:androidx_work_work_runtime_2_6_0",
    ],
)

aar_import_test_suite(
    name = "starlark_aar_import_with_srcjar_tests",
)
