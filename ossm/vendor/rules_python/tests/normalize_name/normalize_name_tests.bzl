# Copyright 2023 The Bazel Authors. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

""

load("@rules_testing//lib:test_suite.bzl", "test_suite")
load("//python/private:normalize_name.bzl", "normalize_name")  # buildifier: disable=bzl-visibility

_tests = []

def _test_name_normalization(env):
    want = {
        input: "friendly_bard"
        for input in [
            "friendly-bard",
            "Friendly-Bard",
            "FRIENDLY-BARD",
            "friendly.bard",
            "friendly_bard",
            "friendly--bard",
            "FrIeNdLy-._.-bArD",
        ]
    }

    actual = {
        input: normalize_name(input)
        for input in want.keys()
    }
    env.expect.that_dict(actual).contains_exactly(want)

_tests.append(_test_name_normalization)

def normalize_name_test_suite(name):
    """Create the test suite.

    Args:
        name: the name of the test suite
    """
    test_suite(name = name, basic_tests = _tests)
