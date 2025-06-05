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

"""Unit tests for the fuzzing engine rules and providers."""

load("@bazel_skylib//lib:new_sets.bzl", "sets")
load("@bazel_skylib//lib:unittest.bzl", "analysistest", "asserts")
load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("@rules_cc//cc/common:cc_info.bzl", "CcInfo")
load(":engine.bzl", "FuzzingEngineInfo", "cc_fuzzing_engine")
load(":util.bzl", "generate_file")

# Shared fixtures.

def _setup_common_stubs():
    cc_library(
        name = "library_stub",
        srcs = [],
        testonly = 1,
    )

    generate_file(
        name = "launcher_stub",
        output = "launcher_stub.sh",
        contents = "echo 'Launcher stub'",
        testonly = 1,
    )

    generate_file(
        name = "data_stub",
        output = "data.txt",
        contents = "Test data stub.",
        testonly = 1,
    )

    generate_file(
        name = "anon_data_stub",
        output = "anon_data.txt",
        contents = "Data stub with no environment variable.",
        testonly = 1,
    )

# Test that the FuzzingEngineInfo provider is populated correctly
# (`provider_contents` stem).

def _provider_contents_test_impl(ctx):
    env = analysistest.begin(ctx)
    target_under_test = analysistest.target_under_test(env)
    asserts.true(
        env,
        CcInfo in target_under_test,
    )
    asserts.equals(
        env,
        "Provider Contents",
        target_under_test[FuzzingEngineInfo].display_name,
    )
    asserts.equals(
        env,
        "fuzzing/private/launcher_stub.sh",
        target_under_test[FuzzingEngineInfo].launcher.short_path,
    )
    asserts.set_equals(
        env,
        sets.make([
            "fuzzing/private/launcher_stub.sh",
            "fuzzing/private/data.txt",
            "fuzzing/private/anon_data.txt",
        ]),
        sets.make([
            file.short_path
            for file in target_under_test[FuzzingEngineInfo].launcher_runfiles.files.to_list()
        ]),
    )
    asserts.set_equals(
        env,
        sets.make([
            ("DATA_STUB_FILE", "fuzzing/private/data.txt"),
        ]),
        sets.make([
            (env_var, file.short_path)
            for env_var, file in target_under_test[FuzzingEngineInfo].launcher_environment.items()
        ]),
    )
    return analysistest.end(env)

provider_contents_test = analysistest.make(_provider_contents_test_impl)

def _test_provider_contents():
    cc_fuzzing_engine(
        name = "provider_contents",
        tags = ["manual"],
        display_name = "Provider Contents",
        library = ":library_stub",
        launcher = ":launcher_stub",
        launcher_data = {
            ":data_stub": "DATA_STUB_FILE",
            ":anon_data_stub": "",
        },
        testonly = 1,
    )

    provider_contents_test(
        name = "provider_contents_test",
        target_under_test = ":provider_contents",
    )

# Test that an empty engine name causes a failure
# (`engine_empty_name` stem).

def _engine_empty_name_test_impl(ctx):
    env = analysistest.begin(ctx)
    asserts.expect_failure(
        env,
        "The display_name attribute of the rule must not be empty",
    )
    return analysistest.end(env)

engine_empty_name_test = analysistest.make(
    _engine_empty_name_test_impl,
    expect_failure = True,
)

def _test_engine_name_empty():
    cc_fuzzing_engine(
        name = "engine_empty_name",
        tags = ["manual"],
        display_name = "",
        library = ":library_stub",
        launcher = ":launcher_stub",
        testonly = 1,
    )

    engine_empty_name_test(
        name = "engine_empty_name_test",
        target_under_test = ":engine_empty_name",
    )

# The entire test suite.

def engine_test_suite(name):
    _setup_common_stubs()
    _test_engine_name_empty()
    _test_provider_contents()

    native.test_suite(
        name = name,
        tests = [
            ":engine_empty_name_test",
            ":provider_contents_test",
        ],
    )
