# Copyright 2020 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Compare rule output to a golden version."""

def golden_test(
        name,
        golden,
        subject):
    """Check that output from a rule matches the expected output.

    Args:
      name: test name
      golden: expected content of subect
      subject: build target
    """
    native.sh_test(
        name = name,
        size = "medium",
        srcs = ["@rules_license//tools:diff_test.sh"],
        args = [
            "$(location %s)" % golden,
            "$(location %s)" % subject,
        ],
        data = [
            subject,
            golden,
        ],
    )

def golden_cmd_test(
        name,
        cmd,
        golden,  # Required
        toolchains = [],
        tools = None,
        srcs = [],  # Optional
        **kwargs):  # Rest
    """Compares cmd output to golden output, passes if they are identical.

    Args:
      name: Name of the build rule.
      cmd: The command to run to generate output.
      golden: The golden file to be compared.
      toolchains: List of toolchains needed to run the command, passed to genrule.
      tools: List of tools needed to run the command, passed to genrule.
      srcs: List of sources needed as input to the command, passed to genrule.
      **kwargs: Any additional parameters for the generated golden_test.
    """
    actual = name + ".output"

    native.genrule(
        name = name + "_output",
        srcs = srcs,
        outs = [actual],
        cmd = cmd + " > '$@'",  # Redirect to collect output
        toolchains = toolchains,
        tools = tools,
        testonly = True,
    )

    golden_test(
        name = name,
        subject = actual,
        golden = golden,
        **kwargs
    )
