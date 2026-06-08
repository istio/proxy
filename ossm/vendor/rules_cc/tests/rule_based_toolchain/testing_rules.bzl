# Copyright 2024 The Bazel Authors. All rights reserved.
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
"""Helpers for creating tests for the rule based toolchain."""

load("@rules_testing//lib:analysis_test.bzl", _analysis_test = "analysis_test")
load("@rules_testing//lib:truth.bzl", "matching")
load("@rules_testing//lib:util.bzl", "util")
load(":subjects.bzl", "FACTORIES")

visibility("//tests/rule_based_toolchain/...")

helper_target = util.helper_target

def analysis_test(*, name, **kwargs):
    """An analysis test for the toolchain rules.

    Args:
      name: (str) The name of the test suite.
      **kwargs: Kwargs to be passed to rules_testing's analysis_test.
    """

    _analysis_test(
        name = name,
        provider_subject_factories = FACTORIES,
        **kwargs
    )

def expect_failure_test(*, name, target, failure_message):
    def _impl(env, target):
        env.expect.that_target(target).failures().contains_predicate(matching.contains(failure_message))

    _analysis_test(
        name = name,
        expect_failure = True,
        impl = _impl,
        target = target,
    )
