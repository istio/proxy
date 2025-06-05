# Copyright 2019 The Bazel Authors. All rights reserved.
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

"""A test verifying other targets can be successfully analyzed as part of a `bazel test`"""

def _analysis_test_impl(ctx):
    """Implementation function for analysis_test. """
    _ignore = [ctx]  # @unused
    return [AnalysisTestResultInfo(
        success = True,
        message = "All targets succeeded analysis",
    )]

analysis_test = rule(
    _analysis_test_impl,
    attrs = {"targets": attr.label_list(mandatory = True)},
    test = True,
    analysis_test = True,
    doc = """Test rule checking that other targets can be successfully analyzed.

    This rule essentially verifies that all targets under `targets` would
    generate no errors when analyzed with `bazel build [targets] --nobuild`.
    Action success/failure for the targets and their transitive dependencies
    are not verified. An analysis test simply ensures that each target in the transitive
    dependencies propagate providers appropriately and register actions for their outputs
    appropriately.

    NOTE: If the targets fail to analyze, instead of the analysis_test failing, the analysis_test
    will fail to build. Ideally, it would instead result in a test failure. This is a current
    infrastructure limitation that may be fixed in the future.

    Typical usage:

      load("@bazel_skylib//rules:analysis_test.bzl", "analysis_test")
      analysis_test(
          name = "my_analysis_test",
          targets = [
              "//some/package:rule",
          ],
      )

    Args:
      name: The name of the test rule.
      targets: A list of targets to ensure build.""",
)
