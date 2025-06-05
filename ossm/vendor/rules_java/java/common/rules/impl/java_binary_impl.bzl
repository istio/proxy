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

load("@rules_cc//cc/common:cc_common.bzl", "cc_common")
load("@rules_cc//cc/common:cc_info.bzl", "CcInfo")
load("@rules_proto//proto:defs.bzl", "ProtoInfo")
load("//java/common:java_common.bzl", "java_common")
load("//java/common:java_info.bzl", "JavaInfo")
load("//java/common:java_semantics.bzl", "semantics")
load("//java/common/rules/impl:basic_java_library_impl.bzl", "basic_java_library", "collect_deps")
load(":java_binary_deploy_jar.bzl", "create_deploy_archive")
load(":java_helper.bzl", "helper")

visibility(["//java/..."])

_java_common_internal = java_common.internal_DO_NOT_USE()
JavaCompilationInfo = _java_common_internal.JavaCompilationInfo
collect_native_deps_dirs = _java_common_internal.collect_native_deps_dirs
get_runtime_classpath_for_archive = _java_common_internal.get_runtime_classpath_for_archive
to_java_binary_info = _java_common_internal.to_java_binary_info

InternalDeployJarInfo = provider(
    "Provider for passing info to deploy jar rule",
    fields = [
        "java_attrs",
        "strip_as_default",
        "add_exports",
        "add_opens",
    ],
)

def basic_java_binary(
        ctx,
        deps,
        runtime_deps,
        resources,
        main_class,
        coverage_main_class,
        coverage_config,
        launcher_info,
        executable,
        strip_as_default,
        extra_java_info = None,
        is_test_rule_class = False):
    """Creates actions for compiling and linting java sources, coverage support, and sources jar (_deploy-src.jar).

    Args:
        ctx: (RuleContext) The rule context
        deps: (list[Target]) The list of other targets to be compiled with
        runtime_deps: (list[Target]) The list of other targets to be linked in
        resources: (list[File]) The list of data files to be included in the class jar
        main_class: (String) FQN of the java main class
        coverage_main_class: (String) FQN of the actual main class if coverage is enabled
        coverage_config: (Struct|None) If coverage is enabled, a struct with fields (runner, manifest, env, support_files), None otherwise
        launcher_info: (Struct) Structure with fields (launcher, unstripped_launcher, runfiles, runtime_jars, jvm_flags, classpath_resources)
        executable: (File) The executable output of the rule
        strip_as_default: (bool) Whether this target outputs a stripped launcher and deploy jar
        extra_java_info: (JavaInfo) additional outputs to merge
        is_test_rule_class: (bool) Whether this rule is a test rule

    Returns:
        Tuple(
            dict[str, Provider],    // providers
            Struct(                 // default info
                files_to_build: depset(File),
                runfiles: Runfiles,
                executable: File
            ),
            list[String]            // jvm flags
          )

    """
    if not ctx.attr.create_executable and (ctx.attr.launcher and cc_common.launcher_provider in ctx.attr.launcher):
        fail("launcher specified but create_executable is false")
    if not ctx.attr.use_launcher and (ctx.attr.launcher and ctx.attr.launcher.label != semantics.LAUNCHER_FLAG_LABEL):
        fail("launcher specified but use_launcher is false")

    if not ctx.attr.srcs and ctx.attr.deps:
        fail("deps not allowed without srcs; move to runtime_deps?")

    module_flags = [dep[JavaInfo].module_flags_info for dep in runtime_deps if JavaInfo in dep]
    add_exports = depset(ctx.attr.add_exports, transitive = [m.add_exports for m in module_flags])
    add_opens = depset(ctx.attr.add_opens, transitive = [m.add_opens for m in module_flags])

    classpath_resources = []
    classpath_resources.extend(launcher_info.classpath_resources)
    if hasattr(ctx.files, "classpath_resources"):
        classpath_resources.extend(ctx.files.classpath_resources)

    toolchain = semantics.find_java_toolchain(ctx)
    timezone_data = [toolchain._timezone_data] if toolchain._timezone_data else []
    target, common_info = basic_java_library(
        ctx,
        srcs = ctx.files.srcs,
        deps = deps,
        runtime_deps = runtime_deps,
        plugins = ctx.attr.plugins,
        resources = resources,
        resource_jars = timezone_data,
        classpath_resources = classpath_resources,
        javacopts = ctx.attr.javacopts,
        neverlink = ctx.attr.neverlink,
        enable_compile_jar_action = False,
        coverage_config = coverage_config,
        add_exports = ctx.attr.add_exports,
        add_opens = ctx.attr.add_opens,
        bootclasspath = ctx.attr.bootclasspath,
    )
    java_info = target["JavaInfo"]
    compilation_info = java_info.compilation_info
    runtime_classpath = depset(
        order = "preorder",
        transitive = [
            java_info.transitive_runtime_jars
            for java_info in (
                collect_deps(ctx.attr.runtime_deps + deps) +
                ([coverage_config.runner] if coverage_config and coverage_config.runner else [])
            )
        ],
    )
    if extra_java_info:
        runtime_classpath = depset(order = "preorder", transitive = [
            extra_java_info.transitive_runtime_jars,
            runtime_classpath,
        ])
        java_info = java_common.merge([java_info, extra_java_info])
        compilation_info = JavaCompilationInfo(
            compilation_classpath = compilation_info.compilation_classpath,
            runtime_classpath = runtime_classpath,
            boot_classpath = compilation_info.boot_classpath,
            javac_options = compilation_info.javac_options,
        )

    java_attrs = _collect_attrs(ctx, runtime_classpath, classpath_resources)

    jvm_flags = []

    jvm_flags.extend(launcher_info.jvm_flags)

    native_libs_depsets = []
    for dep in runtime_deps:
        if JavaInfo in dep:
            native_libs_depsets.append(dep[JavaInfo].transitive_native_libraries)
        if CcInfo in dep:
            native_libs_depsets.append(dep[CcInfo].transitive_native_libraries())
    native_libs_dirs = collect_native_deps_dirs(depset(transitive = native_libs_depsets))
    if native_libs_dirs:
        prefix = "${JAVA_RUNFILES}/" + ctx.workspace_name + "/"
        jvm_flags.append("-Djava.library.path=%s" % (
            ":".join([prefix + d for d in native_libs_dirs])
        ))

    jvm_flags.extend(ctx.fragments.java.default_jvm_opts)
    jvm_flags.extend([ctx.expand_make_variables(
        "jvm_flags",
        ctx.expand_location(flag, ctx.attr.data, short_paths = True),
        {},
    ) for flag in ctx.attr.jvm_flags])

    # TODO(cushon): make string formatting lazier once extend_template support is added
    # https://github.com/bazelbuild/proposals#:~:text=2022%2D04%2D25,Starlark
    jvm_flags.extend(["--add-exports=%s=ALL-UNNAMED" % x for x in add_exports.to_list()])
    jvm_flags.extend(["--add-opens=%s=ALL-UNNAMED" % x for x in add_opens.to_list()])

    files_to_build = []

    if executable:
        files_to_build.append(executable)

    output_groups = common_info.output_groups

    if coverage_config:
        _generate_coverage_manifest(ctx, coverage_config.manifest, java_attrs.runtime_classpath)
        files_to_build.append(coverage_config.manifest)

    if extra_java_info:
        files_to_build.extend(extra_java_info.runtime_output_jars)
        output_groups["_direct_source_jars"] = (
            output_groups["_direct_source_jars"] + extra_java_info.source_jars
        )
        output_groups["_source_jars"] = depset(
            direct = extra_java_info.source_jars,
            transitive = [output_groups["_source_jars"]],
        )

    if (ctx.fragments.java.one_version_enforcement_on_java_tests or not is_test_rule_class):
        one_version_output = _create_one_version_check(ctx, java_attrs.runtime_classpath, is_test_rule_class)
    else:
        one_version_output = None

    validation_outputs = [one_version_output] if one_version_output else []

    _create_deploy_sources_jar(ctx, output_groups["_source_jars"])

    files = depset(files_to_build + common_info.files_to_build)

    transitive_runfiles_artifacts = depset(transitive = [
        files,
        java_attrs.runtime_classpath,
        depset(transitive = launcher_info.runfiles),
    ])

    runfiles = ctx.runfiles(
        transitive_files = transitive_runfiles_artifacts,
        collect_default = True,
    )

    if launcher_info.launcher:
        default_launcher = helper.filter_launcher_for_target(ctx)
        default_launcher_artifact = helper.launcher_artifact_for_target(ctx)
        default_launcher_runfiles = default_launcher[DefaultInfo].default_runfiles
        if default_launcher_artifact == launcher_info.launcher:
            runfiles = runfiles.merge(default_launcher_runfiles)
        else:
            # N.B. The "default launcher" referred to here is the launcher target specified through
            # an attribute or flag. We wish to retain the runfiles of the default launcher, *except*
            # for the original cc_binary artifact, because we've swapped it out with our custom
            # launcher. Hence, instead of calling builder.addTarget(), or adding an odd method
            # to Runfiles.Builder, we "unravel" the call and manually add things to the builder.
            # Because the NestedSet representing each target's launcher runfiles is re-built here,
            # we may see increased memory consumption for representing the target's runfiles.
            runfiles = runfiles.merge(
                ctx.runfiles(
                    files = [launcher_info.launcher],
                    transitive_files = depset([
                        file
                        for file in default_launcher_runfiles.files.to_list()
                        if file != default_launcher_artifact
                    ]),
                    symlinks = default_launcher_runfiles.symlinks,
                    root_symlinks = default_launcher_runfiles.root_symlinks,
                ),
            )

    runfiles = runfiles.merge_all([
        dep[DefaultInfo].default_runfiles
        for dep in ctx.attr.runtime_deps
        if DefaultInfo in dep
    ])

    if validation_outputs:
        output_groups["_validation"] = output_groups.get("_validation", []) + validation_outputs

    _filter_validation_output_group(ctx, output_groups)

    java_binary_info = to_java_binary_info(java_info, compilation_info)

    internal_deploy_jar_info = InternalDeployJarInfo(
        java_attrs = java_attrs,
        strip_as_default = strip_as_default,
        add_exports = add_exports,
        add_opens = add_opens,
    )

    # "temporary" workaround for https://github.com/bazelbuild/intellij/issues/5845
    extra_files = []
    if is_test_rule_class and ctx.fragments.java.auto_create_java_test_deploy_jars():
        extra_files.append(_auto_create_deploy_jar(ctx, internal_deploy_jar_info, launcher_info, main_class, coverage_main_class))

    default_info = struct(
        files = depset(extra_files, transitive = [files]),
        runfiles = runfiles,
        executable = executable,
    )

    return {
        "OutputGroupInfo": OutputGroupInfo(**output_groups),
        "JavaInfo": java_binary_info,
        "InstrumentedFilesInfo": target["InstrumentedFilesInfo"],
        "JavaRuntimeClasspathInfo": java_common.JavaRuntimeClasspathInfo(runtime_classpath = java_info.transitive_runtime_jars),
        "InternalDeployJarInfo": internal_deploy_jar_info,
    }, default_info, jvm_flags

def _collect_attrs(ctx, runtime_classpath, classpath_resources):
    deploy_env_jars = depset(transitive = [
        dep[java_common.JavaRuntimeClasspathInfo].runtime_classpath
        for dep in ctx.attr.deploy_env
    ]) if hasattr(ctx.attr, "deploy_env") else depset()

    runtime_classpath_for_archive = get_runtime_classpath_for_archive(runtime_classpath, deploy_env_jars)
    runtime_jars = [ctx.outputs.classjar]

    resources = [p for p in ctx.files.srcs if p.extension == "properties"]
    transitive_resources = []
    for r in ctx.attr.resources:
        transitive_resources.append(
            r[ProtoInfo].transitive_sources if ProtoInfo in r else r.files,
        )

    resource_names = dict()
    for r in classpath_resources:
        if r.basename in resource_names:
            fail("entries must have different file names (duplicate: %s)" % r.basename)
        resource_names[r.basename] = None

    return struct(
        runtime_jars = depset(runtime_jars),
        runtime_classpath_for_archive = runtime_classpath_for_archive,
        classpath_resources = depset(classpath_resources),
        runtime_classpath = depset(order = "preorder", direct = runtime_jars, transitive = [runtime_classpath]),
        resources = depset(resources, transitive = transitive_resources),
    )

def _generate_coverage_manifest(ctx, output, runtime_classpath):
    ctx.actions.write(
        output = output,
        content = "\n".join([file.short_path for file in runtime_classpath.to_list()]),
    )

def _create_one_version_check(ctx, inputs, is_test_rule_class):
    one_version_level = ctx.fragments.java.one_version_enforcement_level
    if one_version_level == "OFF":
        return None
    tool = helper.check_and_get_one_version_attribute(ctx, "_one_version_tool")

    if is_test_rule_class:
        toolchain = semantics.find_java_toolchain(ctx)
        allowlist = toolchain._one_version_allowlist_for_tests
    else:
        allowlist = helper.check_and_get_one_version_attribute(ctx, "_one_version_allowlist")

    if not tool:  # On Mac oneversion tool is not available
        return None

    output = ctx.actions.declare_file("%s-one-version.txt" % ctx.label.name)

    args = ctx.actions.args()
    args.set_param_file_format("shell").use_param_file("@%s", use_always = True)

    one_version_inputs = []
    args.add("--output", output)
    if allowlist:
        args.add("--allowlist", allowlist)
        one_version_inputs.append(allowlist)
    if one_version_level == "WARNING":
        args.add("--succeed_on_found_violations")
    args.add_all(
        "--inputs",
        inputs,
        map_each = helper.jar_and_target_arg_mapper,
    )

    ctx.actions.run(
        mnemonic = "JavaOneVersion",
        progress_message = "Checking for one-version violations in %{label}",
        executable = tool,
        toolchain = semantics.JAVA_TOOLCHAIN_TYPE,
        inputs = depset(one_version_inputs, transitive = [inputs]),
        tools = [tool],
        outputs = [output],
        arguments = [args],
    )

    return output

def _create_deploy_sources_jar(ctx, sources):
    helper.create_single_jar(
        ctx.actions,
        toolchain = semantics.find_java_toolchain(ctx),
        output = ctx.outputs.deploysrcjar,
        sources = sources,
    )

def _filter_validation_output_group(ctx, output_group):
    to_exclude = depset(transitive = [
        dep[OutputGroupInfo]._validation
        for dep in ctx.attr.deploy_env
        if OutputGroupInfo in dep and hasattr(dep[OutputGroupInfo], "_validation")
    ]) if hasattr(ctx.attr, "deploy_env") else depset()
    if to_exclude:
        transitive_validations = depset(transitive = [
            _get_validations_from_attr(ctx, attr_name)
            for attr_name in dir(ctx.attr)
            # we also exclude implicit, cfg=host/exec and tool attributes
            if not attr_name.startswith("_") and
               attr_name not in [
                   "deploy_env",
                   "applicable_licenses",
                   "package_metadata",
                   "plugins",
                   "translations",
                   # special ignored attributes
                   "compatible_with",
                   "restricted_to",
                   "exec_compatible_with",
                   "target_compatible_with",
               ]
        ])
        if not ctx.attr.create_executable:
            excluded_set = {x: None for x in to_exclude.to_list()}
            transitive_validations = [
                x
                for x in transitive_validations.to_list()
                if x not in excluded_set
            ]
        output_group["_validation_transitive"] = transitive_validations

def _get_validations_from_attr(ctx, attr_name):
    attr = getattr(ctx.attr, attr_name)
    if type(attr) == "list":
        return depset(transitive = [_get_validations_from_target(t) for t in attr])
    else:
        return _get_validations_from_target(attr)

def _get_validations_from_target(target):
    if (
        type(target) == "Target" and
        OutputGroupInfo in target and
        hasattr(target[OutputGroupInfo], "_validation")
    ):
        return target[OutputGroupInfo]._validation
    else:
        return depset()

# TODO: bazelbuild/intellij/issues/5845 - remove this once no longer required
# this need not be completely identical to the regular deploy jar since we only
# care about packaging the classpath
def _auto_create_deploy_jar(ctx, info, launcher_info, main_class, coverage_main_class):
    output = ctx.actions.declare_file(ctx.label.name + "_auto_deploy.jar")
    java_attrs = info.java_attrs
    runtime_classpath = depset(
        direct = launcher_info.runtime_jars,
        transitive = [
            java_attrs.runtime_jars,
            java_attrs.runtime_classpath_for_archive,
        ],
        order = "preorder",
    )
    create_deploy_archive(
        ctx,
        launcher = launcher_info.launcher,
        main_class = main_class,
        coverage_main_class = coverage_main_class,
        resources = java_attrs.resources,
        classpath_resources = java_attrs.classpath_resources,
        runtime_classpath = runtime_classpath,
        manifest_lines = [],
        build_info_files = [],
        build_target = str(ctx.label),
        output = output,
        one_version_level = ctx.fragments.java.one_version_enforcement_level,
        one_version_allowlist = helper.check_and_get_one_version_attribute(ctx, "_one_version_allowlist"),
        multi_release = ctx.fragments.java.multi_release_deploy_jars,
        hermetic = hasattr(ctx.attr, "hermetic") and ctx.attr.hermetic,
        add_exports = info.add_exports,
        add_opens = info.add_opens,
    )
    return output
