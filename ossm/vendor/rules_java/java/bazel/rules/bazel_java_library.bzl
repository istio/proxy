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
Definition of java_library rule.
"""

load("//java/common:java_info.bzl", "JavaInfo")
load("//java/common:java_semantics.bzl", "semantics")
load("//java/common/rules:android_lint.bzl", "android_lint_subrule")
load("//java/common/rules:java_library.bzl", "JAVA_LIBRARY_ATTRS")
load("//java/common/rules/impl:bazel_java_library_impl.bzl", "bazel_java_library_rule")

visibility(["//java/..."])

def _proxy(ctx):
    return bazel_java_library_rule(
        ctx,
        ctx.files.srcs,
        ctx.attr.deps,
        ctx.attr.runtime_deps,
        ctx.attr.plugins,
        ctx.attr.exports,
        ctx.attr.exported_plugins,
        ctx.files.resources,
        ctx.attr.javacopts,
        ctx.attr.neverlink,
        ctx.files.proguard_specs,
        ctx.attr.add_exports,
        ctx.attr.add_opens,
        ctx.attr.bootclasspath,
        ctx.attr.javabuilder_jvm_flags,
    ).values()

java_library = rule(
    _proxy,
    doc = """
<p>This rule compiles and links sources into a <code>.jar</code> file.</p>

<h4>Implicit outputs</h4>
<ul>
  <li><code>lib<var>name</var>.jar</code>: A Java archive containing the class files.</li>
  <li><code>lib<var>name</var>-src.jar</code>: An archive containing the sources ("source
    jar").</li>
</ul>
    """,
    attrs = JAVA_LIBRARY_ATTRS,
    provides = [JavaInfo],
    outputs = {
        "classjar": "lib%{name}.jar",
        "sourcejar": "lib%{name}-src.jar",
    },
    fragments = ["java", "cpp"],
    toolchains = [semantics.JAVA_TOOLCHAIN],
    subrules = [android_lint_subrule],
)
