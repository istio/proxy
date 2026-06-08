# Copyright 2018 The Bazel Authors. All rights reserved.
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

"""Implementation of the `swift_binary` rule."""

load("@bazel_skylib//lib:dicts.bzl", "dicts")
load("@bazel_skylib//lib:paths.bzl", "paths")
load("//swift/internal:compiling.bzl", "compile")
load(
    "//swift/internal:feature_names.bzl",
    "SWIFT_FEATURE_ADD_TARGET_NAME_TO_OUTPUT",
)
load("//swift/internal:features.bzl", "is_feature_enabled")
load(
    "//swift/internal:linking.bzl",
    "binary_rule_attrs",
    "configure_features_for_binary",
    "create_linking_context_from_compilation_outputs",
    "malloc_linking_context",
    "register_link_binary_action",
)
load(
    "//swift/internal:output_groups.bzl",
    "supplemental_compilation_output_groups",
)
load("//swift/internal:providers.bzl", "SwiftCompilerPluginInfo")
load(
    "//swift/internal:toolchain_utils.bzl",
    "get_swift_toolchain",
    "use_swift_toolchain",
)
load(
    "//swift/internal:utils.bzl",
    "expand_locations",
    "get_providers",
    "include_developer_search_paths",
)
load(":module_name.bzl", "derive_swift_module_name")
load(
    ":providers.bzl",
    "SwiftBinaryInfo",
    "SwiftInfo",
    "create_swift_module_context",
)

def _maybe_parse_as_library_copts(srcs):
    """Returns a list of compiler flags depending on `main.swift`'s presence.

    Now that the `@main` attribute exists and is becoming more common, in the
    case there is a single file not named `main.swift`, we assume that it has a
    `@main` annotation, in which case it needs to be parsed as a library, not
    as if it has top level code. In the case this is the wrong assumption,
    compilation or linking will fail.

    Args:
        srcs: A list of source files to check for the presence of `main.swift`.

    Returns:
        A list of compiler flags to add to `copts`
    """
    use_parse_as_library = len(srcs) == 1 and \
                           srcs[0].basename != "main.swift"
    return ["-parse-as-library"] if use_parse_as_library else []

def _swift_binary_impl(ctx):
    swift_toolchain = get_swift_toolchain(ctx)

    feature_configuration = configure_features_for_binary(
        ctx = ctx,
        requested_features = ctx.features,
        swift_toolchain = swift_toolchain,
        unsupported_features = ctx.disabled_features,
    )

    srcs = ctx.files.srcs
    output_groups = {}
    module_contexts = []
    additional_linking_contexts = []

    # If the binary has sources, compile those first and collect the outputs to
    # be passed to the linker.
    if srcs:
        module_name = ctx.attr.module_name
        if not module_name:
            module_name = derive_swift_module_name(ctx.label)
        entry_point_function_name = "{}_main".format(module_name)

        include_dev_srch_paths = include_developer_search_paths(ctx.attr)

        compile_result = compile(
            actions = ctx.actions,
            additional_inputs = ctx.files.swiftc_inputs,
            cc_infos = get_providers(ctx.attr.deps, CcInfo),
            copts = expand_locations(
                ctx,
                ctx.attr.copts,
                ctx.attr.swiftc_inputs,
            ) + _maybe_parse_as_library_copts(srcs) + [
                # Use a custom entry point name so that the binary's code can
                # also be linked into another process (like a test executable)
                # without having its main function collide.
                "-Xfrontend",
                "-entry-point-function-name",
                "-Xfrontend",
                entry_point_function_name,
            ],
            defines = ctx.attr.defines,
            feature_configuration = feature_configuration,
            include_dev_srch_paths = include_dev_srch_paths,
            module_name = module_name,
            objc_infos = get_providers(ctx.attr.deps, apple_common.Objc),
            package_name = ctx.attr.package_name,
            plugins = get_providers(ctx.attr.plugins, SwiftCompilerPluginInfo),
            srcs = srcs,
            swift_infos = get_providers(ctx.attr.deps, SwiftInfo),
            swift_toolchain = swift_toolchain,
            target_name = ctx.label.name,
            workspace_name = ctx.workspace_name,
        )
        module_contexts.append(compile_result.module_context)
        compilation_outputs = compile_result.compilation_outputs
        supplemental_outputs = compile_result.supplemental_outputs
        output_groups = supplemental_compilation_output_groups(
            supplemental_outputs,
        )
    else:
        compile_result = None
        entry_point_function_name = None
        compilation_outputs = cc_common.create_compilation_outputs()

    additional_linking_contexts.append(malloc_linking_context(ctx))

    # Apply the optional debugging outputs extension if the toolchain defines
    # one.
    debug_outputs_provider = swift_toolchain.debug_outputs_provider
    if debug_outputs_provider:
        debug_extension = debug_outputs_provider(ctx = ctx)
        additional_debug_outputs = debug_extension.additional_outputs
        variables_extension = debug_extension.variables_extension
    else:
        additional_debug_outputs = []
        variables_extension = {}

    binary_link_flags = expand_locations(
        ctx,
        ctx.attr.linkopts,
        ctx.attr.swiftc_inputs,
    ) + ctx.fragments.cpp.linkopts

    # When linking the binary, make sure we use the correct entry point name.
    if entry_point_function_name:
        entry_point_linkopts = swift_toolchain.entry_point_linkopts_provider(
            entry_point_name = entry_point_function_name,
        ).linkopts
    else:
        entry_point_linkopts = []

    if is_feature_enabled(
        feature_configuration = feature_configuration,
        feature_name = SWIFT_FEATURE_ADD_TARGET_NAME_TO_OUTPUT,
    ):
        name = paths.join(ctx.label.name, ctx.label.name)
    else:
        name = ctx.label.name

    linking_outputs = register_link_binary_action(
        actions = ctx.actions,
        additional_inputs = ctx.files.swiftc_inputs,
        additional_linking_contexts = additional_linking_contexts,
        additional_outputs = additional_debug_outputs,
        feature_configuration = feature_configuration,
        compilation_outputs = compilation_outputs,
        deps = ctx.attr.deps,
        module_contexts = module_contexts,
        name = name,
        output_type = "executable",
        owner = ctx.label,
        stamp = ctx.attr.stamp,
        swift_toolchain = swift_toolchain,
        user_link_flags = binary_link_flags + entry_point_linkopts,
        variables_extension = variables_extension,
    )

    providers = [
        DefaultInfo(
            executable = linking_outputs.executable,
            files = depset(
                [linking_outputs.executable] + additional_debug_outputs,
            ),
            runfiles = ctx.runfiles(
                collect_data = True,
                collect_default = True,
                files = ctx.files.data,
            ),
        ),
        OutputGroupInfo(**output_groups),
        SwiftInfo(
            modules = [
                create_swift_module_context(
                    name = module_context.name,
                    compilation_context = module_context.compilation_context,
                    # The rest of the fields are intentionally ommited, as we
                    # only want to expose the compilation_context
                )
                for module_context in module_contexts
            ],
        ),
    ]

    # Only create a linking context and propagate `SwiftBinaryInfo` if this rule
    # compiled something (i.e., it had sources). If it didn't, then there's
    # nothing to allow testing against.
    if compile_result:
        linking_context, _ = (
            create_linking_context_from_compilation_outputs(
                actions = ctx.actions,
                additional_inputs = ctx.files.swiftc_inputs,
                alwayslink = True,
                compilation_outputs = compilation_outputs,
                feature_configuration = feature_configuration,
                label = ctx.label,
                linking_contexts = [
                    dep[CcInfo].linking_context
                    for dep in ctx.attr.deps
                    if CcInfo in dep
                ],
                module_context = compile_result.module_context,
                swift_toolchain = swift_toolchain,
                # Exclude the entry point linkopts from this linking context,
                # because it is meant to be used by other binary rules that
                # provide their own entry point while linking this "binary" in
                # as a library.
                user_link_flags = binary_link_flags,
            )
        )
        providers.append(SwiftBinaryInfo(
            cc_info = CcInfo(
                compilation_context = (
                    compile_result.module_context.clang.compilation_context
                ),
                linking_context = linking_context,
            ),
            swift_info = compile_result.swift_info,
        ))

    return providers

swift_binary = rule(
    attrs = dicts.add(
        binary_rule_attrs(
            additional_deps_providers = [[SwiftCompilerPluginInfo]],
            stamp_default = -1,
        ),
        {
            # TODO(b/301253335): Enable AEGs and switch from `swift` exec_group to swift `toolchain` param.
            "_use_auto_exec_groups": attr.bool(default = False),
        },
    ),
    doc = """\
Compiles and links Swift code into an executable binary.

On Linux, this rule produces an executable binary for the desired target
architecture.

On Apple platforms, this rule produces a _single-architecture_ binary; it does
not produce fat binaries. As such, this rule is mainly useful for creating Swift
tools intended to run on the local build machine.

If you want to create a multi-architecture binary or a bundled application,
please use one of the platform-specific application rules in
[rules_apple](https://github.com/bazelbuild/rules_apple) instead of
`swift_binary`.
""",
    executable = True,
    fragments = ["cpp"],
    implementation = _swift_binary_impl,
    toolchains = use_swift_toolchain(),
)
