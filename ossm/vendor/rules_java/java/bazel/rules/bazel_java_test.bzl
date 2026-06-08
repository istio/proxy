# Copyright 2022 The Bazel Authors. All rights reserved.
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
"""Bazel java_test rule"""

load("//java/common:java_semantics.bzl", "semantics")
load("//java/common/rules:java_binary.bzl", "BASE_TEST_ATTRIBUTES")
load("//java/common/rules:rule_util.bzl", "merge_attrs")
load("//java/common/rules/impl:java_helper.bzl", "helper")
load(":bazel_java_binary.bzl", "BASE_BINARY_ATTRS", "bazel_base_binary_impl", "make_binary_rule")

visibility(["//java", "//java/docs"])

def _bazel_java_test_impl(ctx):
    return bazel_base_binary_impl(ctx, is_test_rule_class = True) + helper.test_providers(ctx)

def _java_test_initializer(**kwargs):
    if "stamp" in kwargs and type(kwargs["stamp"]) == type(True):
        kwargs["stamp"] = 1 if kwargs["stamp"] else 0
    if "use_launcher" in kwargs and not kwargs["use_launcher"]:
        kwargs["launcher"] = None
    else:
        # If launcher is not set or None, set it to config flag
        if "launcher" not in kwargs or not kwargs["launcher"]:
            kwargs["launcher"] = semantics.LAUNCHER_FLAG_LABEL
    return kwargs

java_test = make_binary_rule(
    _bazel_java_test_impl,
    doc = """
<p>
A <code>java_test()</code> rule compiles a Java test. A test is a binary wrapper around your
test code. The test runner's main method is invoked instead of the main class being compiled.
</p>

<h4 id="java_test_implicit_outputs">Implicit output targets</h4>
<ul>
  <li><code><var>name</var>.jar</code>: A Java archive.</li>
  <li><code><var>name</var>_deploy.jar</code>: A Java archive suitable
    for deployment. (Only built if explicitly requested.) See the description of the
    <code><var>name</var>_deploy.jar</code> output from
    <a href="#java_binary">java_binary</a> for more details.</li>
</ul>

<p>
See the section on <code>java_binary()</code> arguments. This rule also
supports all <a href="https://bazel.build/reference/be/common-definitions#common-attributes-tests">attributes common
to all test rules (*_test)</a>.
</p>

<h4 id="java_test_examples">Examples</h4>

<pre class="code">
<code class="lang-starlark">

java_library(
    name = "tests",
    srcs = glob(["*.java"]),
    deps = [
        "//java/com/foo/base:testResources",
        "//java/com/foo/testing/util",
    ],
)

java_test(
    name = "AllTests",
    size = "small",
    runtime_deps = [
        ":tests",
        "//util/mysql",
    ],
)
</code>
</pre>
    """,
    attrs = merge_attrs(
        BASE_TEST_ATTRIBUTES,
        BASE_BINARY_ATTRS,
        {
            "_lcov_merger": attr.label(
                cfg = "exec",
                default = configuration_field(
                    fragment = "coverage",
                    name = "output_generator",
                ),
            ),
            "_collect_cc_coverage": attr.label(
                cfg = "exec",
                allow_single_file = True,
                default = "@bazel_tools//tools/test:collect_cc_coverage",
            ),
        },
        override_attrs = {
            "use_testrunner": attr.bool(
                default = True,
                doc = semantics.DOCS.for_attribute("use_testrunner") + """
<br/>
You can use this to override the default
behavior, which is to use test runner for
<code>java_test</code> rules,
and not use it for <code>java_binary</code> rules.  It is unlikely
you will want to do this.  One use is for <code>AllTest</code>
rules that are invoked by another rule (to set up a database
before running the tests, for example).  The <code>AllTest</code>
rule must be declared as a <code>java_binary</code>, but should
still use the test runner as its main entry point.

The name of a test runner class can be overridden with <code>main_class</code> attribute.
                """,
            ),
            "stamp": attr.int(
                default = 0,
                values = [-1, 0, 1],
                doc = """
Whether to encode build information into the binary. Possible values:
<ul>
<li>
  <code>stamp = 1</code>: Always stamp the build information into the binary, even in
  <a href="https://bazel.build/docs/user-manual#stamp"><code>--nostamp</code></a> builds. <b>This
  setting should be avoided</b>, since it potentially kills remote caching for the
  binary and any downstream actions that depend on it.
</li>
<li>
  <code>stamp = 0</code>: Always replace build information by constant values. This
  gives good build result caching.
</li>
<li>
  <code>stamp = -1</code>: Embedding of build information is controlled by the
  <a href="https://bazel.build/docs/user-manual#stamp"><code>--[no]stamp</code></a> flag.
</li>
</ul>
<p>Stamped binaries are <em>not</em> rebuilt unless their dependencies change.</p>
                """,
            ),
        },
        remove_attrs = ["deploy_env"],
    ),
    test = True,
    initializer = _java_test_initializer,
)
