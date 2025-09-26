""" Definition of _java_single_jar. """

load("//java/common:java_common.bzl", "java_common")
load("//java/common:java_info.bzl", "JavaInfo")

def _java_single_jar(ctx):
    transitive_inputs = []
    for dep in ctx.attr.deps:
        if JavaInfo in dep:
            info = dep[JavaInfo]
            transitive_inputs.append(info.transitive_runtime_jars)
            if hasattr(info, "compilation_info"):
                compilation_info = info.compilation_info
                if hasattr(compilation_info, "runtime_classpath"):
                    transitive_inputs.append(compilation_info.runtime_classpath)
        else:
            files = []
            for f in dep[DefaultInfo].files.to_list():
                if not f.extension == "jar":
                    fail("unexpected file type in java_single_jar.deps: %s" % f.path)
                files.append(f)
            transitive_inputs.append(depset(files))
    inputs = depset(transitive = transitive_inputs)

    if hasattr(java_common, "JavaRuntimeClasspathInfo"):
        deploy_env_jars = depset(transitive = [
            dep[java_common.JavaRuntimeClasspathInfo].runtime_classpath
            for dep in ctx.attr.deploy_env
        ])
        excluded_jars = {jar: None for jar in deploy_env_jars.to_list()}
        if excluded_jars:
            inputs = depset([jar for jar in inputs.to_list() if jar not in excluded_jars])

    args = ctx.actions.args()
    args.add_all("--sources", inputs)
    args.use_param_file("@%s")
    args.set_param_file_format("multiline")
    args.add_all("--deploy_manifest_lines", ctx.attr.deploy_manifest_lines)
    args.add("--output", ctx.outputs.jar)
    args.add("--normalize")

    # Deal with limitation of singlejar flags: tool's default behavior is
    # "no", but you get that behavior only by absence of compression flags.
    if ctx.attr.compress == "preserve":
        args.add("--dont_change_compression")
    elif ctx.attr.compress == "yes":
        args.add("--compression")
    elif ctx.attr.compress == "no":
        pass
    else:
        fail("\"compress\" attribute (%s) must be: yes, no, preserve." % ctx.attr.compress)

    if ctx.attr.exclude_build_data:
        args.add("--exclude_build_data")
    if ctx.attr.multi_release:
        args.add("--multi_release")

    ctx.actions.run(
        inputs = inputs,
        outputs = [ctx.outputs.jar],
        arguments = [args],
        progress_message = "Merging into %s" % ctx.outputs.jar.short_path,
        mnemonic = "JavaSingleJar",
        executable = ctx.executable._singlejar,
    )

    files = depset([ctx.outputs.jar])
    providers = [DefaultInfo(
        files = files,
        runfiles = ctx.runfiles(transitive_files = files),
    )]
    if hasattr(java_common, "JavaRuntimeClasspathInfo"):
        providers.append(java_common.JavaRuntimeClasspathInfo(runtime_classpath = inputs))
    return providers

java_single_jar = rule(
    attrs = {
        "deps": attr.label_list(
            allow_files = True,
            doc = """
                The Java targets (including java_import and java_library) to collect
                transitive dependencies from. Runtime dependencies are collected via
                deps, exports, and runtime_deps. Resources are also collected.
                Native cc_library or java_wrap_cc dependencies are not.""",
        ),
        "deploy_manifest_lines": attr.string_list(doc = """
          A list of lines to add to the <code>META-INF/manifest.mf</code> file."""),
        "deploy_env": attr.label_list(
            providers = [java_common.JavaRuntimeClasspathInfo] if hasattr(java_common, "JavaRuntimeClasspathInfo") else [],
            allow_files = False,
            doc = """
            A list of `java_binary` or `java_single_jar` targets which represent
            the deployment environment for this binary.

            Set this attribute when building a plugin which will be loaded by another
            `java_binary`.

            `deploy_env` dependencies are excluded from the jar built by this rule.""",
        ),
        "compress": attr.string(default = "preserve", doc = """
            Whether to always deflate ("yes"), always store ("no"), or pass
            through unmodified ("preserve"). The default is "preserve", and is the
            most efficient option -- no extra work is done to inflate or deflate."""),
        "exclude_build_data": attr.bool(default = True, doc = """
            Whether to omit the build-data.properties file generated
            by default."""),
        "multi_release": attr.bool(default = True, doc = """Whether to enable Multi-Release output jars."""),
        "_singlejar": attr.label(
            default = Label("//toolchains:singlejar"),
            cfg = "exec",
            allow_single_file = True,
            executable = True,
        ),
    },
    outputs = {
        "jar": "%{name}.jar",
    },
    implementation = _java_single_jar,
    doc = """
Collects Java dependencies and jar files into a single jar

`java_single_jar` collects Java dependencies and jar files into a single jar.
This is similar to java_binary with everything related to executables disabled,
and provides an alternative to the java_binary "deploy jar hack".

## Example

```skylark
load("//tools/build_defs/java_single_jar:java_single_jar.bzl", "java_single_jar")

java_single_jar(
    name = "my_single_jar",
    deps = [
        "//java/com/google/foo",
        "//java/com/google/bar",
    ],
)
```

Outputs:
  {name}.jar: A single jar containing all of the inputs.
""",
)
