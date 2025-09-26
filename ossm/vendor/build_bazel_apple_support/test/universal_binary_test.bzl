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

"""Definition of a test rule to test universal_binary."""

# Template for the test script used to validate that the action outputs contain
# the expected values.
_TEST_SCRIPT_CONTENTS = """\
#!/bin/bash

set -euo pipefail

newline=$'\n'

SYMBOLS=(
  {binary_contains_symbols}
)

if [[ ! -f "{file}" ]]; then
  echo "{file} doesn't exist"
  exit 1
fi

actual_symbols=$(nm -Uj -arch x86_64 -arch arm64 {file})
for symbol in "${{SYMBOLS[@]}}"; do
  echo "$actual_symbols" | grep -Fxq "$symbol" || \
    (echo "In file: {file}"; \
     echo "Expected symbol \"$symbol\" was not found. The " \
       "symbols in the binary were:$newline$actual_symbols"; \
     exit 1)
done

echo "Test passed"

exit 0
"""

def _universal_binary_test_transition(_settings, attr):
    """Implementation of the universal_binary_test_transition transition."""

    return {
        "//command_line_option:cpu": attr.cpu,
    }

universal_binary_test_transition = transition(
    implementation = _universal_binary_test_transition,
    inputs = [],
    outputs = ["//command_line_option:cpu"],
)

def _universal_binary_test_impl(ctx):
    """Implementation of the universal_binary_test rule."""

    target_under_test = ctx.split_attr.target_under_test.values()[0]
    output_to_verify = target_under_test[DefaultInfo].files.to_list()[0]

    test_script = ctx.actions.declare_file("{}_test_script".format(ctx.label.name))
    test_script_contents = _TEST_SCRIPT_CONTENTS.format(
        binary_contains_symbols = "\n  ".join([
            x
            for x in ctx.attr.binary_contains_symbols
        ]),
        file = output_to_verify.short_path,
    )

    ctx.actions.write(
        content = test_script_contents,
        is_executable = True,
        output = test_script,
    )

    return [
        DefaultInfo(
            executable = test_script,
            runfiles = ctx.runfiles(files = [output_to_verify]),
        ),
    ]

universal_binary_test = rule(
    attrs = {
        "binary_contains_symbols": attr.string_list(
            doc = """\
A list of symbols that should appear in the binary file specified in
`target_under_test`.""",
            mandatory = True,
        ),
        "cpu": attr.string(
            doc = "CPU to use for test under target.",
        ),
        "target_under_test": attr.label(
            mandatory = True,
            providers = [DefaultInfo],
            doc = "The binary whose contents are to be verified.",
            cfg = universal_binary_test_transition,
        ),
        "_allowlist_function_transition": attr.label(
            default = "@bazel_tools//tools/allowlists/function_transition_allowlist",
        ),
    },
    fragments = ["apple"],
    implementation = _universal_binary_test_impl,
    test = True,
)
