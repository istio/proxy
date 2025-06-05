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

load("@rules_cc//cc/common:cc_info.bzl", "CcInfo")
load("//java/common:java_common.bzl", "java_common")
load("//java/common:java_info.bzl", "JavaInfo")
load("//java/common:java_semantics.bzl", "semantics")
load("//java/common/rules/impl:basic_java_library_impl.bzl", "construct_defaultinfo")
load("//java/common/rules/impl:import_deps_check.bzl", "import_deps_check")
load(":proguard_validation.bzl", "validate_proguard_specs")

visibility(["//java/..."])

_java_common_internal = java_common.internal_DO_NOT_USE()
_run_ijar_private_for_builtins = _java_common_internal.run_ijar_private_for_builtins

def _filter_provider(provider, *attrs):
    return [dep[provider] for attr in attrs for dep in attr if provider in dep]

def _collect_jars(ctx, jars):
    jars_dict = {}
    for info in jars:
        if JavaInfo in info:
            fail("'jars' attribute cannot contain labels of Java targets")
        for jar in info.files.to_list():
            jar_path = jar.dirname + jar.basename
            if jars_dict.get(jar_path) != None:
                fail("in jars attribute of java_import rule //" + ctx.label.package + ":" + ctx.attr.name + ": " + jar.basename + " is a duplicate")
            jars_dict[jar_path] = jar
    return [jar_tuple[1] for jar_tuple in jars_dict.items()] if len(jars_dict.items()) > 0 else []

def _process_with_ijars_if_needed(jars, ctx):
    file_dict = {}
    use_ijars = ctx.fragments.java.use_ijars()
    for jar in jars:
        interface_jar = jar
        if use_ijars:
            ijar_basename = jar.short_path.removeprefix("../").removesuffix("." + jar.extension) + "-ijar.jar"
            interface_jar_directory = "_ijar/" + ctx.label.name + "/" + ijar_basename

            interface_jar = ctx.actions.declare_file(interface_jar_directory)
            _run_ijar_private_for_builtins(
                ctx.actions,
                target_label = ctx.label,
                jar = jar,
                output = interface_jar,
                java_toolchain = semantics.find_java_toolchain(ctx),
            )
        file_dict[jar] = interface_jar

    return file_dict

def _check_export_error(ctx, exports):
    not_in_allowlist = hasattr(ctx.attr, "_allowlist_java_import_exports") and not getattr(ctx.attr, "_allowlist_java_import_exports")[PackageSpecificationInfo].contains(ctx.label)
    disallow_java_import_exports = ctx.fragments.java.disallow_java_import_exports()

    if len(exports) != 0 and (disallow_java_import_exports or not_in_allowlist):
        fail("java_import.exports is no longer supported; use java_import.deps instead")

def _check_empty_jars_error(ctx, jars):
    # TODO(kotlaja): Remove temporary incompatible flag [disallow_java_import_empty_jars] once migration is done.
    not_in_allowlist = hasattr(ctx.attr, "_allowlist_java_import_empty_jars") and not getattr(ctx.attr, "_allowlist_java_import_empty_jars")[PackageSpecificationInfo].contains(ctx.label)
    disallow_java_import_empty_jars = ctx.fragments.java.disallow_java_import_empty_jars()

    if len(jars) == 0 and disallow_java_import_empty_jars and not_in_allowlist:
        fail("empty java_import.jars is no longer supported " + ctx.label.package)

def _create_java_info_with_dummy_output_file(ctx, srcjar, all_deps, exports, runtime_deps_list, neverlink, cc_info_list, add_exports, add_opens):
    dummy_jar = ctx.actions.declare_file(ctx.label.name + "_dummy.jar")
    dummy_src_jar = srcjar
    if dummy_src_jar == None:
        dummy_src_jar = ctx.actions.declare_file(ctx.label.name + "_src_dummy.java")
        ctx.actions.write(dummy_src_jar, "")
    return java_common.compile(
        ctx,
        output = dummy_jar,
        java_toolchain = semantics.find_java_toolchain(ctx),
        source_files = [dummy_src_jar],
        deps = all_deps,
        runtime_deps = runtime_deps_list,
        neverlink = neverlink,
        exports = [export[JavaInfo] for export in exports if JavaInfo in export],  # Watchout, maybe you need to add them there manually.
        native_libraries = cc_info_list,
        add_exports = add_exports,
        add_opens = add_opens,
    )

def bazel_java_import_rule(
        ctx,
        jars = [],
        srcjar = None,
        deps = [],
        runtime_deps = [],
        exports = [],
        neverlink = False,
        proguard_specs = [],
        add_exports = [],
        add_opens = []):
    """Implements java_import.

    This rule allows the use of precompiled .jar files as libraries in other Java rules.

    Args:
      ctx: (RuleContext) Used to register the actions.
      jars: (list[Artifact]) List of output jars.
      srcjar: (Artifact) The jar containing the sources.
      deps: (list[Target]) The list of dependent libraries.
      runtime_deps: (list[Target]) Runtime dependencies to attach to the rule.
      exports: (list[Target])  The list of exported libraries.
      neverlink: (bool) Whether this rule should only be used for compilation and not at runtime.
      proguard_specs: (list[File]) Files to be used as Proguard specification.
      add_exports: (list[str]) Allow this library to access the given <module>/<package>.
      add_opens: (list[str]) Allow this library to reflectively access the given <module>/<package>.

    Returns:
      (list[provider]) A list containing DefaultInfo, JavaInfo,
      OutputGroupsInfo, ProguardSpecProvider providers.
    """

    _check_empty_jars_error(ctx, jars)
    _check_export_error(ctx, exports)

    collected_jars = _collect_jars(ctx, jars)
    all_deps = _filter_provider(JavaInfo, deps, exports)

    jdeps_artifact = None
    merged_java_info = java_common.merge(all_deps)
    not_in_allowlist = hasattr(ctx.attr, "_allowlist_java_import_deps_checking") and not ctx.attr._allowlist_java_import_deps_checking[PackageSpecificationInfo].contains(ctx.label)
    if len(collected_jars) > 0 and not_in_allowlist and "incomplete-deps" not in ctx.attr.tags:
        jdeps_artifact = import_deps_check(
            ctx,
            collected_jars,
            merged_java_info.compile_jars,
            merged_java_info.transitive_compile_time_jars,
            "java_import",
        )

    compilation_to_runtime_jar_map = _process_with_ijars_if_needed(collected_jars, ctx)
    runtime_deps_list = [runtime_dep[JavaInfo] for runtime_dep in runtime_deps if JavaInfo in runtime_dep]
    cc_info_list = [dep[CcInfo] for dep in deps if CcInfo in dep]
    java_info = None
    if len(collected_jars) > 0:
        java_infos = []
        for jar in collected_jars:
            java_infos.append(JavaInfo(
                output_jar = jar,
                compile_jar = compilation_to_runtime_jar_map[jar],
                deps = all_deps,
                runtime_deps = runtime_deps_list,
                neverlink = neverlink,
                source_jar = srcjar,
                exports = [export[JavaInfo] for export in exports if JavaInfo in export],  # Watchout, maybe you need to add them there manually.
                native_libraries = cc_info_list,
                add_exports = add_exports,
                add_opens = add_opens,
            ))
        java_info = java_common.merge(java_infos)
    else:
        # TODO(kotlaja): Remove next line once all java_import targets with empty jars attribute are cleaned from depot (b/246559727).
        java_info = _create_java_info_with_dummy_output_file(ctx, srcjar, all_deps, exports, runtime_deps_list, neverlink, cc_info_list, add_exports, add_opens)

    target = {"JavaInfo": java_info}

    target["ProguardSpecProvider"] = validate_proguard_specs(
        ctx,
        proguard_specs,
        [deps, runtime_deps, exports],
    )

    # TODO(kotlaja): Revise if collected_runtimes can be added into construct_defaultinfo directly.
    collected_runtimes = []
    for runtime_dep in ctx.attr.runtime_deps:
        collected_runtimes.extend(runtime_dep.files.to_list())

    target["DefaultInfo"] = construct_defaultinfo(
        ctx,
        collected_jars,
        collected_jars + collected_runtimes,
        neverlink,
        exports,
    )

    output_group_src_jars = depset() if srcjar == None else depset([srcjar])
    target["OutputGroupInfo"] = OutputGroupInfo(
        **{
            "_source_jars": output_group_src_jars,
            "_direct_source_jars": output_group_src_jars,
            "_validation": depset() if jdeps_artifact == None else depset([jdeps_artifact]),
            "_hidden_top_level_INTERNAL_": target["ProguardSpecProvider"].specs,
        }
    )
    return target
