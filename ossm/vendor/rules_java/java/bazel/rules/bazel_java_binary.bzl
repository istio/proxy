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
"""Bazel java_binary rule"""

load("@bazel_skylib//lib:paths.bzl", "paths")
load("@rules_cc//cc:find_cc_toolchain.bzl", "use_cc_toolchain")
load("//java/common:java_info.bzl", "JavaInfo")
load("//java/common:java_semantics.bzl", "semantics")
load(
    "//java/common/rules:android_lint.bzl",
    "android_lint_subrule",
)
load("//java/common/rules:java_binary.bzl", "BASIC_JAVA_BINARY_ATTRIBUTES")
load("//java/common/rules:rule_util.bzl", "merge_attrs")
load("//java/common/rules/impl:java_binary_deploy_jar.bzl", "create_deploy_archives")
load("//java/common/rules/impl:java_binary_impl.bzl", "basic_java_binary")
load("//java/common/rules/impl:java_helper.bzl", "helper")

visibility("//java/docs")

def _bazel_java_binary_impl(ctx):
    return bazel_base_binary_impl(ctx, is_test_rule_class = False) + helper.executable_providers(ctx)

def bazel_base_binary_impl(ctx, is_test_rule_class):
    """Common implementation for binaries and tests

    Args:
        ctx: (RuleContext)
        is_test_rule_class: (bool)

    Returns:
        [Provider]
    """
    deps = _collect_all_targets_as_deps(ctx, classpath_type = "compile_only")
    runtime_deps = _collect_all_targets_as_deps(ctx)

    main_class = _check_and_get_main_class(ctx)
    coverage_main_class = main_class
    coverage_config = helper.get_coverage_config(ctx, _get_coverage_runner(ctx))
    if coverage_config:
        main_class = coverage_config.main_class

    launcher_info = _get_launcher_info(ctx)

    executable = _get_executable(ctx)

    feature_config = helper.get_feature_config(ctx)
    if feature_config:
        strip_as_default = helper.should_strip_as_default(ctx, feature_config)
    else:
        # No C++ toolchain available.
        strip_as_default = False

    providers, default_info, jvm_flags = basic_java_binary(
        ctx,
        deps,
        runtime_deps,
        ctx.files.resources,
        main_class,
        coverage_main_class,
        coverage_config,
        launcher_info,
        executable,
        strip_as_default,
        is_test_rule_class = is_test_rule_class,
    )

    if ctx.attr.use_testrunner:
        if semantics.find_java_runtime_toolchain(ctx).version >= 17:
            jvm_flags.append("-Djava.security.manager=allow")
        test_class = ctx.attr.test_class if hasattr(ctx.attr, "test_class") else ""
        if test_class == "":
            test_class = helper.primary_class(ctx)
        if test_class == None:
            fail("cannot determine test class. You might want to rename the " +
                 " rule or add a 'test_class' attribute.")
        jvm_flags.extend([
            "-ea",
            "-Dbazel.test_suite=" + helper.shell_escape(test_class),
        ])

    java_attrs = providers["InternalDeployJarInfo"].java_attrs

    if executable:
        _create_stub(ctx, java_attrs, launcher_info.launcher, executable, jvm_flags, main_class, coverage_main_class)

    runfiles = default_info.runfiles

    if executable:
        runtime_toolchain = semantics.find_java_runtime_toolchain(ctx)
        runfiles = runfiles.merge(ctx.runfiles(transitive_files = runtime_toolchain.files))

    test_support = helper.get_test_support(ctx)
    if test_support:
        runfiles = runfiles.merge(test_support[DefaultInfo].default_runfiles)

    providers["DefaultInfo"] = DefaultInfo(
        files = default_info.files,
        runfiles = runfiles,
        executable = default_info.executable,
    )

    info = providers.pop("InternalDeployJarInfo")
    create_deploy_archives(
        ctx,
        info.java_attrs,
        launcher_info,
        main_class,
        coverage_main_class,
        info.strip_as_default,
        add_exports = info.add_exports,
        add_opens = info.add_opens,
    )

    return providers.values()

def _get_coverage_runner(ctx):
    if ctx.configuration.coverage_enabled and ctx.attr.create_executable:
        toolchain = semantics.find_java_toolchain(ctx)
        runner = toolchain.jacocorunner
        if not runner:
            fail("jacocorunner not set in java_toolchain: %s" % toolchain.label)
        runner_jar = runner.executable

        # wrap the jar in JavaInfo so we can add it to deps for java_common.compile()
        return JavaInfo(output_jar = runner_jar, compile_jar = runner_jar)

    return None

def _collect_all_targets_as_deps(ctx, classpath_type = "all"):
    deps = helper.collect_all_targets_as_deps(ctx, classpath_type = classpath_type)

    if classpath_type == "compile_only" and ctx.fragments.java.enforce_explicit_java_test_deps():
        return deps

    test_support = helper.get_test_support(ctx)
    if test_support:
        deps.append(test_support)
    return deps

def _check_and_get_main_class(ctx):
    create_executable = ctx.attr.create_executable
    main_class = _get_main_class(ctx)

    if not create_executable and main_class:
        fail("main class must not be specified when executable is not created")
    if create_executable and not main_class:
        if not ctx.attr.srcs:
            fail("need at least one of 'main_class' or Java source files")
        main_class = helper.primary_class(ctx)
        if main_class == None:
            fail("main_class was not provided and cannot be inferred: " +
                 "source path doesn't include a known root (java, javatests, src, testsrc)")

    return _get_main_class(ctx)

def _get_main_class(ctx):
    if not ctx.attr.create_executable:
        return None

    main_class = _get_main_class_from_rule(ctx)

    if main_class == "":
        main_class = helper.primary_class(ctx)
    return main_class

def _get_main_class_from_rule(ctx):
    main_class = ctx.attr.main_class
    if main_class:
        return main_class
    if ctx.attr.use_testrunner:
        return "com.google.testing.junit.runner.BazelTestRunner"
    return main_class

def _get_launcher_info(ctx):
    launcher = helper.launcher_artifact_for_target(ctx)
    return struct(
        launcher = launcher,
        unstripped_launcher = launcher,
        runfiles = [],
        runtime_jars = [],
        jvm_flags = [],
        classpath_resources = [],
    )

def _get_executable(ctx):
    if not ctx.attr.create_executable:
        return None
    executable_name = ctx.label.name
    if helper.is_target_platform_windows(ctx):
        executable_name = executable_name + ".exe"

    return ctx.actions.declare_file(executable_name)

def _create_stub(ctx, java_attrs, launcher, executable, jvm_flags, main_class, coverage_main_class):
    java_runtime_toolchain = semantics.find_java_runtime_toolchain(ctx)
    java_executable = helper.get_java_executable(ctx, java_runtime_toolchain, launcher)
    workspace_name = ctx.workspace_name
    workspace_prefix = workspace_name + ("/" if workspace_name else "")
    runfiles_enabled = helper.runfiles_enabled(ctx)
    coverage_enabled = ctx.configuration.coverage_enabled

    test_support = helper.get_test_support(ctx)
    test_support_jars = test_support[JavaInfo].transitive_runtime_jars if test_support else depset()
    classpath = depset(
        transitive = [
            java_attrs.runtime_classpath,
            test_support_jars if ctx.fragments.java.enforce_explicit_java_test_deps() else depset(),
        ],
    )

    if helper.is_target_platform_windows(ctx):
        jvm_flags_for_launcher = []
        for flag in jvm_flags:
            jvm_flags_for_launcher.extend(ctx.tokenize(flag))
        return _create_windows_exe_launcher(ctx, java_executable, classpath, main_class, jvm_flags_for_launcher, runfiles_enabled, executable)

    if runfiles_enabled:
        prefix = "" if helper.is_absolute_target_platform_path(ctx, java_executable) else "${JAVA_RUNFILES}/"
        java_bin = "JAVABIN=${JAVABIN:-" + prefix + java_executable + "}"
    else:
        java_bin = "JAVABIN=${JAVABIN:-$(rlocation " + java_executable + ")}"

    td = ctx.actions.template_dict()
    td.add_joined(
        "%classpath%",
        classpath,
        map_each = lambda file: _format_classpath_entry(runfiles_enabled, workspace_prefix, file),
        join_with = ctx.configuration.host_path_separator,
        format_joined = "\"%s\"",
        allow_closure = True,
    )

    ctx.actions.expand_template(
        template = ctx.file._stub_template,
        output = executable,
        substitutions = {
            "%runfiles_manifest_only%": "" if runfiles_enabled else "1",
            "%workspace_prefix%": workspace_prefix,
            "%javabin%": java_bin,
            "%needs_runfiles%": "0" if helper.is_absolute_target_platform_path(ctx, java_runtime_toolchain.java_executable_exec_path) else "1",
            "%set_jacoco_metadata%": "",
            "%set_jacoco_main_class%": "export JACOCO_MAIN_CLASS=" + coverage_main_class if coverage_enabled else "",
            "%set_jacoco_java_runfiles_root%": "export JACOCO_JAVA_RUNFILES_ROOT=${JAVA_RUNFILES}/" + workspace_prefix if coverage_enabled else "",
            "%java_start_class%": helper.shell_escape(main_class),
            "%jvm_flags%": " ".join(jvm_flags),
        },
        computed_substitutions = td,
        is_executable = True,
    )
    return executable

def _format_classpath_entry(runfiles_enabled, workspace_prefix, file):
    if runfiles_enabled:
        return "${RUNPATH}" + file.short_path

    return "$(rlocation " + paths.normalize(workspace_prefix + file.short_path) + ")"

def _create_windows_exe_launcher(ctx, java_executable, classpath, main_class, jvm_flags_for_launcher, runfiles_enabled, executable):
    launch_info = ctx.actions.args().use_param_file("%s", use_always = True).set_param_file_format("multiline")
    launch_info.add("binary_type=Java")
    launch_info.add(ctx.workspace_name, format = "workspace_name=%s")
    launch_info.add("1" if runfiles_enabled else "0", format = "symlink_runfiles_enabled=%s")
    launch_info.add(java_executable, format = "java_bin_path=%s")
    launch_info.add(main_class, format = "java_start_class=%s")
    launch_info.add_joined(classpath, map_each = _short_path, join_with = ";", format_joined = "classpath=%s", omit_if_empty = False)
    launch_info.add_joined(jvm_flags_for_launcher, join_with = "\t", format_joined = "jvm_flags=%s", omit_if_empty = False)
    launch_info.add(semantics.find_java_runtime_toolchain(ctx).java_home_runfiles_path, format = "jar_bin_path=%s/bin/jar.exe")

    # TODO(b/295221112): Change to use the "launcher" attribute (only windows use a fixed _launcher attribute)
    launcher_artifact = ctx.executable._launcher
    ctx.actions.run(
        executable = ctx.executable._windows_launcher_maker,
        inputs = [launcher_artifact],
        outputs = [executable],
        arguments = [launcher_artifact.path, launch_info, executable.path],
        use_default_shell_env = True,
    )
    return executable

def _short_path(file):
    return file.short_path

def _compute_test_support(use_testrunner):
    return Label(semantics.JAVA_TEST_RUNNER_LABEL) if use_testrunner else None

def make_binary_rule(implementation, *, doc, attrs, executable = False, test = False, initializer = None):
    return rule(
        implementation = implementation,
        initializer = initializer,
        doc = doc,
        attrs = attrs,
        executable = executable,
        test = test,
        fragments = ["cpp", "java"],
        provides = [JavaInfo],
        toolchains = [semantics.JAVA_TOOLCHAIN] + use_cc_toolchain() + (
            [semantics.JAVA_RUNTIME_TOOLCHAIN] if executable or test else []
        ),
        # TODO(hvd): replace with filegroups?
        outputs = {
            "classjar": "%{name}.jar",
            "sourcejar": "%{name}-src.jar",
            "deploysrcjar": "%{name}_deploy-src.jar",
            "deployjar": "%{name}_deploy.jar",
            "unstrippeddeployjar": "%{name}_deploy.jar.unstripped",
        },
        exec_groups = {
            "cpp_link": exec_group(toolchains = use_cc_toolchain()),
        },
        subrules = [android_lint_subrule],
    )

BASE_BINARY_ATTRS = merge_attrs(
    BASIC_JAVA_BINARY_ATTRIBUTES,
    {
        "resource_strip_prefix": attr.string(
            doc = """
The path prefix to strip from Java resources.
<p>
If specified, this path prefix is stripped from every file in the <code>resources</code>
attribute. It is an error for a resource file not to be under this directory. If not
specified (the default), the path of resource file is determined according to the same
logic as the Java package of source files. For example, a source file at
<code>stuff/java/foo/bar/a.txt</code> will be located at <code>foo/bar/a.txt</code>.
</p>
            """,
        ),
        "_test_support": attr.label(default = _compute_test_support),
        "_launcher": attr.label(
            cfg = "exec",
            executable = True,
            default = "@bazel_tools//tools/launcher:launcher",
        ),
        "_windows_launcher_maker": attr.label(
            default = "@bazel_tools//tools/launcher:launcher_maker",
            cfg = "exec",
            executable = True,
        ),
    },
)

def make_java_binary(executable):
    return make_binary_rule(
        _bazel_java_binary_impl,
        doc = """
<p>
  Builds a Java archive ("jar file"), plus a wrapper shell script with the same name as the rule.
  The wrapper shell script uses a classpath that includes, among other things, a jar file for each
  library on which the binary depends. When running the wrapper shell script, any nonempty
  <code>JAVABIN</code> environment variable will take precedence over the version specified via
  Bazel's <code>--java_runtime_version</code> flag.
</p>
<p>
  The wrapper script accepts several unique flags. Refer to
  <code>//src/main/java/com/google/devtools/build/lib/bazel/rules/java/java_stub_template.txt</code>
  for a list of configurable flags and environment variables accepted by the wrapper.
</p>

<h4 id="java_binary_implicit_outputs">Implicit output targets</h4>
<ul>
  <li><code><var>name</var>.jar</code>: A Java archive, containing the class files and other
    resources corresponding to the binary's direct dependencies.</li>
  <li><code><var>name</var>-src.jar</code>: An archive containing the sources ("source
    jar").</li>
  <li><code><var>name</var>_deploy.jar</code>: A Java archive suitable for deployment (only
    built if explicitly requested).
    <p>
      Building the <code>&lt;<var>name</var>&gt;_deploy.jar</code> target for your rule
      creates a self-contained jar file with a manifest that allows it to be run with the
      <code>java -jar</code> command or with the wrapper script's <code>--singlejar</code>
      option. Using the wrapper script is preferred to <code>java -jar</code> because it
      also passes the <a href="#java_binary-jvm_flags">JVM flags</a> and the options
      to load native libraries.
    </p>
    <p>
      The deploy jar contains all the classes that would be found by a classloader that
      searched the classpath from the binary's wrapper script from beginning to end. It also
      contains the native libraries needed for dependencies. These are automatically loaded
      into the JVM at runtime.
    </p>
    <p>If your target specifies a <a href="#java_binary.launcher">launcher</a>
      attribute, then instead of being a normal JAR file, the _deploy.jar will be a
      native binary. This will contain the launcher plus any native (C++) dependencies of
      your rule, all linked into a static binary. The actual jar file's bytes will be
      appended to that native binary, creating a single binary blob containing both the
      executable and the Java code. You can execute the resulting jar file directly
      like you would execute any native binary.</p>
  </li>
  <li><code><var>name</var>_deploy-src.jar</code>: An archive containing the sources
    collected from the transitive closure of the target. These will match the classes in the
    <code>deploy.jar</code> except where jars have no matching source jar.</li>
</ul>

<p>
It is good practice to use the name of the source file that is the main entry point of the
application (minus the extension). For example, if your entry point is called
<code>Main.java</code>, then your name could be <code>Main</code>.
</p>

<p>
  A <code>deps</code> attribute is not allowed in a <code>java_binary</code> rule without
  <a href="#java_binary-srcs"><code>srcs</code></a>; such a rule requires a
  <a href="#java_binary-main_class"><code>main_class</code></a> provided by
  <a href="#java_binary-runtime_deps"><code>runtime_deps</code></a>.
</p>

<p>The following code snippet illustrates a common mistake:</p>

<pre class="code">
<code class="lang-starlark">
java_binary(
    name = "DontDoThis",
    srcs = [
        <var>...</var>,
        <code class="deprecated">"GeneratedJavaFile.java"</code>,  # a generated .java file
    ],
    deps = [<code class="deprecated">":generating_rule",</code>],  # rule that generates that file
)
</code>
</pre>

<p>Do this instead:</p>

<pre class="code">
<code class="lang-starlark">
java_binary(
    name = "DoThisInstead",
    srcs = [
        <var>...</var>,
        ":generating_rule",
    ],
)
</code>
</pre>
        """,
        attrs = merge_attrs(
            BASE_BINARY_ATTRS,
            ({} if executable else {
                "args": attr.string_list(),
                "output_licenses": attr.string_list(),
            }),
        ),
        executable = executable,
    )

java_binary = make_java_binary(executable = True)
