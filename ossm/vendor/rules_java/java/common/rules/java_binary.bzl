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

""" Implementation of java_binary for bazel """

load("@bazel_skylib//lib:paths.bzl", "paths")
load("@rules_cc//cc/common:cc_info.bzl", "CcInfo")
load("//java/common:java_common.bzl", "java_common")
load("//java/common:java_info.bzl", "JavaInfo")
load("//java/common:java_plugin_info.bzl", "JavaPluginInfo")
load("//java/common:java_semantics.bzl", "PLATFORMS_ROOT", "semantics")
load(":basic_java_library.bzl", "BASIC_JAVA_LIBRARY_IMPLICIT_ATTRS")
load(":rule_util.bzl", "merge_attrs")

visibility(["//java/..."])

BootClassPathInfo = java_common.BootClassPathInfo

_java_common_internal = java_common.internal_DO_NOT_USE()

BASIC_JAVA_BINARY_ATTRIBUTES = merge_attrs(
    BASIC_JAVA_LIBRARY_IMPLICIT_ATTRS,
    # buildifier: disable=attr-licenses
    {
        "srcs": attr.label_list(
            allow_files = [".java", ".srcjar", ".properties"] + semantics.EXTRA_SRCS_TYPES,
            flags = ["DIRECT_COMPILE_TIME_INPUT", "ORDER_INDEPENDENT"],
            doc = """
The list of source files that are processed to create the target.
This attribute is almost always required; see exceptions below.
<p>
Source files of type <code>.java</code> are compiled. In case of generated
<code>.java</code> files it is generally advisable to put the generating rule's name
here instead of the name of the file itself. This not only improves readability but
makes the rule more resilient to future changes: if the generating rule generates
different files in the future, you only need to fix one place: the <code>outs</code> of
the generating rule. You should not list the generating rule in <code>deps</code>
because it is a no-op.
</p>
<p>
Source files of type <code>.srcjar</code> are unpacked and compiled. (This is useful if
you need to generate a set of <code>.java</code> files with a genrule.)
</p>
<p>
Rules: if the rule (typically <code>genrule</code> or <code>filegroup</code>) generates
any of the files listed above, they will be used the same way as described for source
files.
</p>

<p>
This argument is almost always required, except if a
<a href="#java_binary.main_class"><code>main_class</code></a> attribute specifies a
class on the runtime classpath or you specify the <code>runtime_deps</code> argument.
</p>
            """,
        ),
        "deps": attr.label_list(
            allow_files = [".jar"],
            allow_rules = semantics.ALLOWED_RULES_IN_DEPS + semantics.ALLOWED_RULES_IN_DEPS_WITH_WARNING,
            providers = [
                [CcInfo],
                [JavaInfo],
            ],
            flags = ["SKIP_ANALYSIS_TIME_FILETYPE_CHECK"],
            doc = """
The list of other libraries to be linked in to the target.
See general comments about <code>deps</code> at
<a href="common-definitions.html#typical-attributes">Typical attributes defined by
most build rules</a>.
            """,
        ),
        "resources": attr.label_list(
            allow_files = True,
            flags = ["SKIP_CONSTRAINTS_OVERRIDE", "ORDER_INDEPENDENT"],
            doc = """
A list of data files to include in a Java jar.

<p>
Resources may be source files or generated files.
</p>
            """ + semantics.DOCS.for_attribute("resources"),
        ),
        "runtime_deps": attr.label_list(
            allow_files = [".jar"],
            allow_rules = semantics.ALLOWED_RULES_IN_DEPS,
            providers = [[CcInfo], [JavaInfo]],
            flags = ["SKIP_ANALYSIS_TIME_FILETYPE_CHECK"],
            doc = """
Libraries to make available to the final binary or test at runtime only.
Like ordinary <code>deps</code>, these will appear on the runtime classpath, but unlike
them, not on the compile-time classpath. Dependencies needed only at runtime should be
listed here. Dependency-analysis tools should ignore targets that appear in both
<code>runtime_deps</code> and <code>deps</code>.
            """,
        ),
        "data": attr.label_list(
            allow_files = True,
            flags = ["SKIP_CONSTRAINTS_OVERRIDE"],
            doc = """
The list of files needed by this library at runtime.
See general comments about <code>data</code>
at <a href="${link common-definitions#typical-attributes}">Typical attributes defined by
most build rules</a>.
            """ + semantics.DOCS.for_attribute("data"),
        ),
        "plugins": attr.label_list(
            providers = [JavaPluginInfo],
            allow_files = True,
            cfg = "exec",
            doc = """
Java compiler plugins to run at compile-time.
Every <code>java_plugin</code> specified in this attribute will be run whenever this rule
is built. A library may also inherit plugins from dependencies that use
<code><a href="#java_library.exported_plugins">exported_plugins</a></code>. Resources
generated by the plugin will be included in the resulting jar of this rule.
            """,
        ),
        "deploy_env": attr.label_list(
            providers = [java_common.JavaRuntimeClasspathInfo],
            allow_files = False,
            doc = """
A list of other <code>java_binary</code> targets which represent the deployment
environment for this binary.
Set this attribute when building a plugin which will be loaded by another
<code>java_binary</code>.<br/> Setting this attribute excludes all dependencies from
the runtime classpath (and the deploy jar) of this binary that are shared between this
binary and the targets specified in <code>deploy_env</code>.
            """,
        ),
        "launcher": attr.label(
            # TODO(b/295221112): add back CcLauncherInfo
            allow_files = False,
            doc = """
Specify a binary that will be used to run your Java program instead of the
normal <code>bin/java</code> program included with the JDK.
The target must be a <code>cc_binary</code>. Any <code>cc_binary</code> that
implements the
<a href="http://docs.oracle.com/javase/7/docs/technotes/guides/jni/spec/invocation.html">
Java Invocation API</a> can be specified as a value for this attribute.

<p>By default, Bazel will use the normal JDK launcher (bin/java or java.exe).</p>

<p>The related <a href="${link user-manual#flag--java_launcher}"><code>
--java_launcher</code></a> Bazel flag affects only those
<code>java_binary</code> and <code>java_test</code> targets that have
<i>not</i> specified a <code>launcher</code> attribute.</p>

<p>Note that your native (C++, SWIG, JNI) dependencies will be built differently
depending on whether you are using the JDK launcher or another launcher:</p>

<ul>
<li>If you are using the normal JDK launcher (the default), native dependencies are
built as a shared library named <code>{name}_nativedeps.so</code>, where
<code>{name}</code> is the <code>name</code> attribute of this java_binary rule.
Unused code is <em>not</em> removed by the linker in this configuration.</li>

<li>If you are using any other launcher, native (C++) dependencies are statically
linked into a binary named <code>{name}_nativedeps</code>, where <code>{name}</code>
is the <code>name</code> attribute of this java_binary rule. In this case,
the linker will remove any code it thinks is unused from the resulting binary,
which means any C++ code accessed only via JNI may not be linked in unless
that <code>cc_library</code> target specifies <code>alwayslink = 1</code>.</li>
</ul>

<p>When using any launcher other than the default JDK launcher, the format
of the <code>*_deploy.jar</code> output changes. See the main
<a href="#java_binary">java_binary</a> docs for details.</p>
            """,
        ),
        "bootclasspath": attr.label(
            providers = [BootClassPathInfo],
            flags = ["SKIP_CONSTRAINTS_OVERRIDE"],
            doc = "Restricted API, do not use!",
        ),
        "neverlink": attr.bool(),
        "javacopts": attr.string_list(
            doc = """
Extra compiler options for this binary.
Subject to <a href="make-variables.html">"Make variable"</a> substitution and
<a href="common-definitions.html#sh-tokenization">Bourne shell tokenization</a>.
<p>These compiler options are passed to javac after the global compiler options.</p>
            """,
        ),
        "add_exports": attr.string_list(
            doc = """
Allow this library to access the given <code>module</code> or <code>package</code>.
<p>
This corresponds to the javac and JVM --add-exports= flags.
            """,
        ),
        "add_opens": attr.string_list(
            doc = """
Allow this library to reflectively access the given <code>module</code> or
<code>package</code>.
<p>
This corresponds to the javac and JVM --add-opens= flags.
            """,
        ),
        "main_class": attr.string(
            doc = """
Name of class with <code>main()</code> method to use as entry point.
If a rule uses this option, it does not need a <code>srcs=[...]</code> list.
Thus, with this attribute one can make an executable from a Java library that already
contains one or more <code>main()</code> methods.
<p>
The value of this attribute is a class name, not a source file. The class must be
available at runtime: it may be compiled by this rule (from <code>srcs</code>) or
provided by direct or transitive dependencies (through <code>runtime_deps</code> or
<code>deps</code>). If the class is unavailable, the binary will fail at runtime; there
is no build-time check.
</p>
            """,
        ),
        "jvm_flags": attr.string_list(
            doc = """
A list of flags to embed in the wrapper script generated for running this binary.
Subject to <a href="${link make-variables#location}">$(location)</a> and
<a href="make-variables.html">"Make variable"</a> substitution, and
<a href="common-definitions.html#sh-tokenization">Bourne shell tokenization</a>.

<p>The wrapper script for a Java binary includes a CLASSPATH definition
(to find all the dependent jars) and invokes the right Java interpreter.
The command line generated by the wrapper script includes the name of
the main class followed by a <code>"$@"</code> so you can pass along other
arguments after the classname.  However, arguments intended for parsing
by the JVM must be specified <i>before</i> the classname on the command
line.  The contents of <code>jvm_flags</code> are added to the wrapper
script before the classname is listed.</p>

<p>Note that this attribute has <em>no effect</em> on <code>*_deploy.jar</code>
outputs.</p>
            """,
        ),
        "deploy_manifest_lines": attr.string_list(
            doc = """
A list of lines to add to the <code>META-INF/manifest.mf</code> file generated for the
<code>*_deploy.jar</code> target. The contents of this attribute are <em>not</em> subject
to <a href="make-variables.html">"Make variable"</a> substitution.
            """,
        ),
        "stamp": attr.int(
            default = -1,
            values = [-1, 0, 1],
            doc = """
Whether to encode build information into the binary. Possible values:
<ul>
<li>
  <code>stamp = 1</code>: Always stamp the build information into the binary, even in
  <a href="${link user-manual#flag--stamp}"><code>--nostamp</code></a> builds. <b>This
  setting should be avoided</b>, since it potentially kills remote caching for the
  binary and any downstream actions that depend on it.
</li>
<li>
  <code>stamp = 0</code>: Always replace build information by constant values. This
  gives good build result caching.
</li>
<li>
  <code>stamp = -1</code>: Embedding of build information is controlled by the
  <a href="${link user-manual#flag--stamp}"><code>--[no]stamp</code></a> flag.
</li>
</ul>
<p>Stamped binaries are <em>not</em> rebuilt unless their dependencies change.</p>
            """,
        ),
        "use_testrunner": attr.bool(
            default = False,
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
        "use_launcher": attr.bool(
            default = True,
            doc = """
Whether the binary should use a custom launcher.

<p>If this attribute is set to false, the
<a href="${link java_binary.launcher}">launcher</a> attribute  and the related
<a href="${link user-manual#flag--java_launcher}"><code>--java_launcher</code></a> flag
will be ignored for this target.
            """,
        ),
        "env": attr.string_dict(),
        "classpath_resources": attr.label_list(
            allow_files = True,
            doc = """
<em class="harmful">DO NOT USE THIS OPTION UNLESS THERE IS NO OTHER WAY)</em>
<p>
A list of resources that must be located at the root of the java tree. This attribute's
only purpose is to support third-party libraries that require that their resources be
found on the classpath as exactly <code>"myconfig.xml"</code>. It is only allowed on
binaries and not libraries, due to the danger of namespace conflicts.
</p>
            """,
        ),
        "licenses": attr.license() if hasattr(attr, "license") else attr.string_list(),
        "_stub_template": attr.label(
            default = semantics.JAVA_STUB_TEMPLATE_LABEL,
            allow_single_file = True,
        ),
        "_java_toolchain_type": attr.label(default = semantics.JAVA_TOOLCHAIN_TYPE),
        "_windows_constraints": attr.label_list(
            default = [paths.join(PLATFORMS_ROOT, "os:windows")],
        ),
        "_build_info_translator": attr.label(default = semantics.BUILD_INFO_TRANSLATOR_LABEL),
    } | ({} if _java_common_internal.incompatible_disable_non_executable_java_binary() else {"create_executable": attr.bool(default = True, doc = "Deprecated, use <code>java_single_jar</code> instead.")}),
)

BASE_TEST_ATTRIBUTES = {
    "test_class": attr.string(
        doc = """
The Java class to be loaded by the test runner.<br/>
<p>
  By default, if this argument is not defined then the legacy mode is used and the
  test arguments are used instead. Set the <code>--nolegacy_bazel_java_test</code> flag
  to not fallback on the first argument.
</p>
<p>
  This attribute specifies the name of a Java class to be run by
  this test. It is rare to need to set this. If this argument is omitted,
  it will be inferred using the target's <code>name</code> and its
  source-root-relative path. If the test is located outside a known
  source root, Bazel will report an error if <code>test_class</code>
  is unset.
</p>
<p>
  For JUnit3, the test class needs to either be a subclass of
  <code>junit.framework.TestCase</code> or it needs to have a public
  static <code>suite()</code> method that returns a
  <code>junit.framework.Test</code> (or a subclass of <code>Test</code>).
  For JUnit4, the class needs to be annotated with
  <code>org.junit.runner.RunWith</code>.
</p>
<p>
  This attribute allows several <code>java_test</code> rules to
  share the same <code>Test</code>
  (<code>TestCase</code>, <code>TestSuite</code>, ...).  Typically
  additional information is passed to it
  (e.g. via <code>jvm_flags=['-Dkey=value']</code>) so that its
  behavior differs in each case, such as running a different
  subset of the tests.  This attribute also enables the use of
  Java tests outside the <code>javatests</code> tree.
</p>
        """,
    ),
    "env_inherit": attr.string_list(),
    "_apple_constraints": attr.label_list(
        default = [
            paths.join(PLATFORMS_ROOT, "os:ios"),
            paths.join(PLATFORMS_ROOT, "os:macos"),
            paths.join(PLATFORMS_ROOT, "os:tvos"),
            paths.join(PLATFORMS_ROOT, "os:visionos"),
            paths.join(PLATFORMS_ROOT, "os:watchos"),
        ],
    ),
    "_legacy_any_type_attrs": attr.string_list(default = ["stamp"]),
}
