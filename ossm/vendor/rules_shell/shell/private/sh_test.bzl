# Copyright 2024 The Bazel Authors. All rights reserved.
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

"""sh_test rule definition."""

load(":sh_executable.bzl", "make_sh_executable_rule")

# For doc generation only.
visibility("public")

sh_test = make_sh_executable_rule(
    test = True,
    fragments = ["coverage"],
    extra_attrs = {
        "_lcov_merger": attr.label(
            cfg = "exec",
            default = configuration_field(fragment = "coverage", name = "output_generator"),
            executable = True,
        ),
        # Add the script as an attribute in order for sh_test to output code coverage results for
        # code covered by CC binaries invocations.
        "_collect_cc_coverage": attr.label(
            cfg = "exec",
            default = "@bazel_tools//tools/test:collect_cc_coverage",
            executable = True,
        ),
    },
)
