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
    doc = """
<p>A <code>sh_test()</code> rule creates a test written as a Bourne shell script.</p>

<p>See the <a href="${link common-definitions#common-attributes-tests}">
attributes common to all test rules (*_test)</a>.</p>

<h4 id="sh_test_examples">Examples</h4>

<pre class="code">
sh_test(
    name = "foo_integration_test",
    size = "small",
    srcs = ["foo_integration_test.sh"],
    deps = [":foo_sh_lib"],
    data = glob(["testdata/*.txt"]),
)
</pre>
""",
    test = True,
    fragments = ["coverage"],
    extra_attrs = {
        "_lcov_merger": attr.label(
            cfg = config.exec(exec_group = "test"),
            default = configuration_field(fragment = "coverage", name = "output_generator"),
            executable = True,
        ),
        # Add the script as an attribute in order for sh_test to output code coverage results for
        # code covered by CC binaries invocations.
        "_collect_cc_coverage": attr.label(
            cfg = config.exec(exec_group = "test"),
            default = "@bazel_tools//tools/test:collect_cc_coverage",
            executable = True,
        ),
    },
)
