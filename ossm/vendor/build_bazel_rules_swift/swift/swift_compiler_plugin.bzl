# Copyright 2023 The Bazel Authors. All rights reserved.
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

"""Implementation of the `swift_compiler_plugin` rule."""

load("@bazel_skylib//lib:dicts.bzl", "dicts")
load("@build_bazel_apple_support//lib:apple_support.bzl", "apple_support")
load("@build_bazel_apple_support//lib:lipo.bzl", "lipo")
load(
    "@build_bazel_apple_support//lib:transitions.bzl",
    "macos_universal_transition",
)
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
)
load(":module_name.bzl", "derive_swift_module_name")
load(":providers.bzl", "SwiftBinaryInfo", "SwiftInfo")

def _swift_compiler_plugin_impl(ctx):
    swift_toolchain = get_swift_toolchain(ctx)

    feature_configuration = configure_features_for_binary(
        ctx = ctx,
        requested_features = ctx.features,
        swift_toolchain = swift_toolchain,
        unsupported_features = ctx.disabled_features,
    )

    deps = ctx.attr.deps
    srcs = ctx.files.srcs
    module_contexts = []

    if not srcs:
        fail("A compiler plugin must have at least one file in 'srcs'.")

    module_name = ctx.attr.module_name
    if not module_name:
        module_name = derive_swift_module_name(ctx.label)
    entry_point_function_name = "{}_main".format(module_name)

    compile_result = compile(
        actions = ctx.actions,
        additional_inputs = ctx.files.swiftc_inputs,
        cc_infos = get_providers(deps, CcInfo),
        copts = expand_locations(
            ctx,
            ctx.attr.copts,
            ctx.attr.swiftc_inputs,
        ) + [
            # Compiler plugins always define a `CompilerPlugin`-conforming type
            # that is attributed with `@main`.
            "-parse-as-library",
            # Use a custom entry point name so that the macro can also be linked
            # into another process (like a test executable) without having its
            # main function collide.
            "-Xfrontend",
            "-entry-point-function-name",
            "-Xfrontend",
            entry_point_function_name,
        ],
        defines = ctx.attr.defines,
        feature_configuration = feature_configuration,
        include_dev_srch_paths = ctx.attr.testonly,
        module_name = module_name,
        objc_infos = get_providers(deps, apple_common.Objc),
        package_name = ctx.attr.package_name,
        plugins = get_providers(ctx.attr.plugins, SwiftCompilerPluginInfo),
        srcs = srcs,
        swift_infos = get_providers(deps, SwiftInfo),
        swift_toolchain = swift_toolchain,
        target_name = ctx.label.name,
        workspace_name = ctx.workspace_name,
    )
    module_context = compile_result.module_context
    module_contexts.append(module_context)
    compilation_outputs = compile_result.compilation_outputs
    supplemental_outputs = compile_result.supplemental_outputs

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

    if is_feature_enabled(
        feature_configuration = feature_configuration,
        feature_name = SWIFT_FEATURE_ADD_TARGET_NAME_TO_OUTPUT,
    ):
        # Making executable in a folder to avoid naming collisions
        name = "{}/{}".format(ctx.label.name, ctx.label.name)
    else:
        name = ctx.label.name

    binary_linking_outputs = register_link_binary_action(
        actions = ctx.actions,
        additional_inputs = ctx.files.swiftc_inputs,
        additional_linking_contexts = [malloc_linking_context(ctx)],
        additional_outputs = additional_debug_outputs,
        compilation_outputs = compilation_outputs,
        deps = deps,
        feature_configuration = feature_configuration,
        module_contexts = module_contexts,
        name = name,
        output_type = "executable",
        owner = ctx.label,
        stamp = ctx.attr.stamp,
        swift_toolchain = swift_toolchain,
        user_link_flags = expand_locations(
            ctx,
            ctx.attr.linkopts,
            ctx.attr.swiftc_inputs,
        ) + ctx.fragments.cpp.linkopts + (
            # When linking the plugin binary, make sure we use the correct entry
            # point name.
            swift_toolchain.entry_point_linkopts_provider(
                entry_point_name = entry_point_function_name,
            ).linkopts
        ),
        variables_extension = variables_extension,
    )

    linking_context, _ = (
        create_linking_context_from_compilation_outputs(
            actions = ctx.actions,
            additional_inputs = ctx.files.swiftc_inputs,
            alwayslink = True,
            compilation_outputs = compilation_outputs,
            feature_configuration = feature_configuration,
            label = ctx.label,
            include_dev_srch_paths = ctx.attr.testonly,
            linking_contexts = [
                dep[CcInfo].linking_context
                for dep in deps
                if CcInfo in dep
            ],
            module_context = module_context,
            swift_toolchain = swift_toolchain,
            user_link_flags = ctx.attr.linkopts,
        )
    )

    return [
        DefaultInfo(
            executable = binary_linking_outputs.executable,
            files = depset(
                [binary_linking_outputs.executable] + additional_debug_outputs,
            ),
            runfiles = ctx.runfiles(
                collect_data = True,
                collect_default = True,
                files = ctx.files.data,
            ),
        ),
        OutputGroupInfo(
            **supplemental_compilation_output_groups(supplemental_outputs)
        ),
        SwiftBinaryInfo(
            cc_info = CcInfo(
                compilation_context = module_context.clang.compilation_context,
                linking_context = linking_context,
            ),
            swift_info = compile_result.swift_info,
        ),
        SwiftCompilerPluginInfo(
            executable = binary_linking_outputs.executable,
            module_names = depset([module_name]),
        ),
    ]

swift_compiler_plugin = rule(
    attrs = dicts.add(
        # Do not stamp macro binaries by default to prevent frequent rebuilds.
        binary_rule_attrs(stamp_default = 0),
    ),
    doc = """\
Compiles and links a Swift compiler plugin (for example, a macro).

A compiler plugin is a standalone executable that minimally implements the
`CompilerPlugin` protocol from the `SwiftCompilerPlugin` module in swift-syntax.
As of the time of this writing (Xcode 15.0), a compiler plugin can contain one
or more macros, which can be associated with other Swift targets to perform
syntax-tree-based expansions.

When a `swift_compiler_plugin` target is listed in the `plugins` attribute of a
`swift_library`, it will be loaded by that library and any targets that directly
depend on it. (The `plugins` attribute also exists on `swift_binary`,
`swift_test`, and `swift_compiler_plugin` itself, to support plugins that are
only used within those targets.)

Compiler plugins also support being built as a library so that they can be
tested. The `swift_test` rule can contain `swift_compiler_plugin` targets in its
`deps`, and the plugin's module can be imported by the test's sources so that
unit tests can be written against the plugin.

Example:

```bzl
# The actual macro code, using SwiftSyntax
swift_compiler_plugin(
    name = "Macros",
    srcs = glob(["Macros/*.swift"]),
    deps = [
        "@SwiftSyntax",
        "@SwiftSyntax//:SwiftCompilerPlugin",
        "@SwiftSyntax//:SwiftSyntaxMacros",
    ],
)

# A target testing the macro itself
swift_test(
    name = "MacrosTests",
    srcs = glob(["MacrosTests/*.swift"]),
    deps = [
        ":Macros",
        "@SwiftSyntax//:SwiftSyntaxMacrosTestSupport",
    ],
)

# The library that defines the macro hook for use in your project
swift_library(
    name = "MacroLibrary",
    srcs = glob(["MacroLibrary/*.swift"]),
    plugins = [":Macros"],
)

# A consumer of the macro library. This doesn't have to be separate from the
# MacroLibrary depending on what makes sense for your project's organization
swift_library(
    name = "MacroConsumer",
    srcs = glob(["Sources/*.swift"]),
    deps = [":MacroLibrary"],
)
```
""",
    executable = True,
    fragments = ["cpp"],
    implementation = _swift_compiler_plugin_impl,
    toolchains = use_swift_toolchain(),
)

def _universal_swift_compiler_plugin_impl(ctx):
    inputs = [
        plugin.files.to_list()[0]
        for plugin in ctx.split_attr.plugin.values()
    ]

    if not inputs:
        fail("Target (%s) `plugin` label ('%s') does not provide any " +
             "file for universal binary" % (ctx.attr.name, ctx.attr.plugin))

    output = ctx.actions.declare_file(ctx.label.name)
    if len(inputs) > 1:
        lipo.create(
            actions = ctx.actions,
            apple_fragment = ctx.fragments.apple,
            inputs = inputs,
            output = output,
            xcode_config = ctx.attr._xcode_config[apple_common.XcodeVersionConfig],
        )
    else:
        ctx.actions.symlink(target_file = inputs[0], output = output)

    cc_infos = []
    direct_swift_modules = []
    module_name = None
    swift_infos = []
    for plugin in ctx.split_attr.plugin.values():
        cc_infos.append(plugin[SwiftBinaryInfo].cc_info)
        direct_swift_modules.extend(plugin[SwiftBinaryInfo].swift_info.direct_modules)
        module_name = plugin[SwiftCompilerPluginInfo].module_names.to_list()[0]
        swift_infos.append(plugin[SwiftBinaryInfo].swift_info)

    first_output_group_info = ctx.split_attr.plugin.values()[0][OutputGroupInfo]
    combined_output_group_info = {}
    for key in first_output_group_info:
        all_values = []
        for plugin in ctx.split_attr.plugin.values():
            all_values.append(plugin[OutputGroupInfo][key])
        combined_output_group_info[key] = depset(transitive = all_values)

    transitive_runfiles = [
        plugin[DefaultInfo].default_runfiles
        for plugin in ctx.split_attr.plugin.values()
    ]

    return [
        DefaultInfo(
            executable = output,
            files = depset([output]),
            runfiles = ctx.runfiles().merge_all(transitive_runfiles),
        ),
        OutputGroupInfo(**combined_output_group_info),
        SwiftBinaryInfo(
            cc_info = cc_common.merge_cc_infos(cc_infos = cc_infos),
            swift_info = SwiftInfo(
                modules = direct_swift_modules,
                swift_infos = swift_infos,
            ),
        ),
        SwiftCompilerPluginInfo(
            executable = output,
            module_names = depset([module_name]),
        ),
    ]

universal_swift_compiler_plugin = rule(
    attrs = dicts.add(
        apple_support.action_required_attrs(),
        {
            "plugin": attr.label(
                cfg = macos_universal_transition,
                doc = "Target to generate a 'fat' binary from.",
                mandatory = True,
                providers = [[SwiftBinaryInfo, SwiftCompilerPluginInfo]],
            ),
            "_allowlist_function_transition": attr.label(
                default = Label(
                    "@bazel_tools//tools/allowlists/function_transition_allowlist",
                ),
            ),
            # TODO(b/301253335): Enable AEGs and switch from `swift` exec_group to swift `toolchain` param.
            "_use_auto_exec_groups": attr.bool(default = False),
        },
    ),
    doc = """\
Wraps an existing `swift_compiler_plugin` target to produce a universal binary.

This is useful to allow sharing of caches between Intel and Apple Silicon Macs
at the cost of building everything twice.

Example:

```bzl
# The actual macro code, using SwiftSyntax, as usual.
swift_compiler_plugin(
    name = "Macros",
    srcs = glob(["Macros/*.swift"]),
    deps = [
        "@SwiftSyntax",
        "@SwiftSyntax//:SwiftCompilerPlugin",
        "@SwiftSyntax//:SwiftSyntaxMacros",
    ],
)

# Wrap your compiler plugin in this universal shim.
universal_swift_compiler_plugin(
    name = "Macros.universal",
    plugin = ":Macros",
)

# The library that defines the macro hook for use in your project, this
# references the universal_swift_compiler_plugin.
swift_library(
    name = "MacroLibrary",
    srcs = glob(["MacroLibrary/*.swift"]),
    plugins = [":Macros.universal"],
)
```
""",
    executable = True,
    fragments = ["cpp", "apple"],
    implementation = _universal_swift_compiler_plugin_impl,
)
