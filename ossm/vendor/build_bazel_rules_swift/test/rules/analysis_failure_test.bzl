# Copyright 2020 The Bazel Authors. All rights reserved.
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

"""Rules for testing analysis-time failures."""

load(
    "@bazel_skylib//lib:unittest.bzl",
    "analysistest",
    "asserts",
)

def _analysis_failure_test_impl(ctx):
    env = analysistest.begin(ctx)
    asserts.expect_failure(env, ctx.attr.expected_message)
    return analysistest.end(env)

def make_analysis_failure_test_rule(config_settings = {}):
    """Returns a `analysis_failure_test`-like rule with custom config settings.

    Args:
        config_settings: A dictionary of configuration settings and their values
            that should be applied during tests.

    Returns:
        A rule returned by `analysistest.make` that has the
        `analysis_failure_test` interface and the given config settings.
    """
    return analysistest.make(
        _analysis_failure_test_impl,
        attrs = {
            "expected_message": attr.string(
                mandatory = True,
                doc = """\
The expected failure message (passed in a call to `fail()` by the rule under
test).
""",
            ),
        },
        config_settings = config_settings,
        expect_failure = True,
    )

# A default instantiation of the rule when no custom config settings are needed.
analysis_failure_test = make_analysis_failure_test_rule()
