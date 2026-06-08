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
"""Definition of a test rule to test xcode_support."""

load(
    "//lib:apple_support.bzl",
    "apple_support",
)
load(
    "//lib:xcode_support.bzl",
    "xcode_support",
)

# Template for the test script used to validate that the action outputs contain the expected
# values.
_TEST_SCRIPT_CONTENTS = """
#!/bin/bash

set -eu

if [[ "{past_version_is_true}" != "True" ]]; then
  echo "FAILURE: 'past_version_is_true' should be True, but it's {past_version_is_true}."
  exit 1
fi

if [[ "{future_version_is_false}" != "False" ]]; then
  echo "FAILURE: 'future_version_is_false' should be False, but it's {future_version_is_false}."
  exit 1
fi
"""

def _xcode_support_test_impl(ctx):
    """Implementation of the xcode_support_test rule."""

    test_script = ctx.actions.declare_file("{}_test_script".format(ctx.label.name))
    xcode_config = ctx.attr._xcode_config[apple_common.XcodeVersionConfig]
    ctx.actions.write(test_script, _TEST_SCRIPT_CONTENTS.format(
        past_version_is_true = str(xcode_support.is_xcode_at_least_version(xcode_config, "1.0")),
        future_version_is_false = str(xcode_support.is_xcode_at_least_version(xcode_config, "999")),
    ), is_executable = True)

    return [DefaultInfo(executable = test_script)]

xcode_support_test = rule(
    implementation = _xcode_support_test_impl,
    attrs = apple_support.action_required_attrs(),
    test = True,
)
