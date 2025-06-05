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

"""
Definition of java_import rule.
"""

load("//java/common:java_info.bzl", "JavaInfo")
load("//java/common:java_semantics.bzl", "semantics")
load("//java/common/rules:java_import.bzl", "JAVA_IMPORT_ATTRS")
load("//java/common/rules/impl:bazel_java_import_impl.bzl", "bazel_java_import_rule")

visibility(["//java", "//java/docs"])

def _proxy(ctx):
    return bazel_java_import_rule(
        ctx,
        ctx.attr.jars,
        ctx.file.srcjar,
        ctx.attr.deps,
        ctx.attr.runtime_deps,
        ctx.attr.exports,
        ctx.attr.neverlink,
        ctx.files.proguard_specs,
        ctx.attr.add_exports,
        ctx.attr.add_opens,
    ).values()

java_import = rule(
    _proxy,
    doc = """
<p>
  This rule allows the use of precompiled <code>.jar</code> files as
  libraries for <code><a href="#java_library">java_library</a></code> and
  <code>java_binary</code> rules.
</p>

<h4 id="java_import_examples">Examples</h4>

<pre class="code">
<code class="lang-starlark">
    java_import(
        name = "maven_model",
        jars = [
            "maven_model/maven-aether-provider-3.2.3.jar",
            "maven_model/maven-model-3.2.3.jar",
            "maven_model/maven-model-builder-3.2.3.jar",
        ],
    )
</code>
</pre>
    """,
    attrs = JAVA_IMPORT_ATTRS,
    provides = [JavaInfo],
    fragments = ["java", "cpp"],
    toolchains = [semantics.JAVA_TOOLCHAIN],
)
