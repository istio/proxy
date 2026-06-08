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
load("@rules_java//java:java_import.bzl", "java_import")
load(":jvm_import_test.bzl", "jvm_import_test_suite")

java_import(
    name = "java_import_that_consumes_the_downloaded_file_directly",
    # Will produce an error if the downloaded file does not have the `.jar` file extension
    jars = ["@com_google_guava_guava_27_0_jre//file"],
)

build_test(
    name = "test_does_jar_artifact_have_the_correct_file_extension",
    targets = [
        ":java_import_that_consumes_the_downloaded_file_directly",
    ],
)

jvm_import_test_suite(name = "jvm_import_tests")
