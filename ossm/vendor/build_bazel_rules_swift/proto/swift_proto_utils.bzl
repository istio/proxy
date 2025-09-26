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

"""
Utilities for proto rules.
"""

load(
    "@bazel_skylib//lib:paths.bzl",
    "paths",
)
load(
    "//swift:providers.bzl",
    "SwiftInfo",
    "SwiftProtoCompilerInfo",
    "SwiftProtoInfo",
)
load("//swift:swift_common.bzl", "swift_common")

# buildifier: disable=bzl-visibility
load(
    "//swift/internal:linking.bzl",
    "new_objc_provider",
)

# buildifier: disable=bzl-visibility
load(
    "//swift/internal:output_groups.bzl",
    "supplemental_compilation_output_groups",
)

# buildifier: disable=bzl-visibility
load(
    "//swift/internal:utils.bzl",
    "get_providers",
    "include_developer_search_paths",
)

def proto_path(proto_src, proto_info):
    """Derives the string used to import the proto.

    This is the proto source path within its repository,
    adjusted by `import_prefix` and `strip_import_prefix`.

    Args:
        proto_src: the proto source File.
        proto_info: the ProtoInfo provider.

    Returns:
        An import path string.
    """
    if proto_info.proto_source_root == ".":
        # true if proto sources were generated
        prefix = proto_src.root.path + "/"
    elif proto_info.proto_source_root.startswith(proto_src.root.path):
        # sometimes true when import paths are adjusted with import_prefix
        prefix = proto_info.proto_source_root + "/"
    else:
        # usually true when paths are not adjusted
        prefix = paths.join(proto_src.root.path, proto_info.proto_source_root) + "/"
    if not proto_src.path.startswith(prefix):
        # sometimes true when importing multiple adjusted protos
        return proto_src.path
    return proto_src.path[len(prefix):]

def register_module_mapping_write_action(*, actions, label, module_mappings):
    """Registers an action that generates a module mapping for a proto library.

    Args:
        actions: The actions object used to declare the files to be generated and the action that generates it.
        label: The label of the target for which the files are being generated.
        module_mappings: The sequence of module mapping `struct`s to be rendered.
            This sequence should already have duplicates removed.

    Returns:
        The `File` representing the module mapping that will be generated in
        protobuf text format.
    """
    mapping_file = actions.declare_file(
        "{}.protoc_gen_swift_modules.asciipb".format(label.name),
    )
    content = "".join([_render_text_module_mapping(m) for m in module_mappings])

    actions.write(
        content = content,
        output = mapping_file,
    )

    return mapping_file

def _render_text_module_mapping(mapping):
    """Renders the text format proto for a module mapping.

    Args:
        mapping: A single module mapping `struct`.

    Returns:
        A string containing the module mapping for the target in protobuf text
        format.
    """
    module_name = mapping.module_name
    proto_file_paths = mapping.proto_file_paths

    content = "mapping {\n"
    content += '  module_name: "%s"\n' % module_name
    if len(proto_file_paths) == 1:
        content += '  proto_file_path: "%s"\n' % proto_file_paths[0]
    else:
        # Use list form to avoid parsing and looking up the field name for each
        # entry.
        content += '  proto_file_path: [\n    "%s"' % proto_file_paths[0]
        for path in proto_file_paths[1:]:
            content += ',\n    "%s"' % path
        content += "\n  ]\n"
    content += "}\n"

    return content

def _generate_module_mappings(
        *,
        bundled_proto_paths,
        module_name,
        proto_infos,
        transitive_swift_proto_deps):
    """Generates module mappings from ProtoInfo and SwiftProtoInfo providers.

    Args:
        bundled_proto_paths: Set (dict) of proto paths bundled with the runtime.
        module_name: Name of the module the direct proto dependencies will be compiled into.
        proto_infos: List of ProtoInfo providers for the direct proto dependencies.
        transitive_swift_proto_deps: Transitive dependencies propagating SwiftProtoInfo providers.

    Returns:
        List of module mappings.
    """

    # Collect the direct proto source files from the proto deps and build the module mapping:
    direct_proto_file_paths = []
    for proto_info in proto_infos:
        proto_file_paths = [
            proto_path(proto_src, proto_info)
            for proto_src in proto_info.check_deps_sources.to_list()
        ]
        direct_proto_file_paths.extend([
            proto_path
            for proto_path in proto_file_paths
            if proto_path not in bundled_proto_paths
        ])
    module_mapping = struct(
        module_name = module_name,
        proto_file_paths = direct_proto_file_paths,
    )

    # Collect the transitive module mappings:
    transitive_module_mappings = []
    for dep in transitive_swift_proto_deps:
        if not SwiftProtoInfo in dep:
            continue
        transitive_module_mappings.extend(dep[SwiftProtoInfo].module_mappings)

    # Create a list combining the direct + transitive module mappings:
    if len(direct_proto_file_paths) > 0:
        return [module_mapping] + transitive_module_mappings
    return transitive_module_mappings

SwiftProtoCcInfo = provider(
    doc = """\
Wraps a `CcInfo` provider added to a `proto_library` by the Swift proto aspect.

This is necessary because `proto_library` targets already propagate a `CcInfo`
provider for C++ protos, so the Swift proto aspect cannot directly attach its
own. (It's also not good practice to attach providers that you don't own to
arbitrary targets, because you don't know how those targets might change in the
future.) The `swift_proto_library` rule will pick up this provider and return
the underlying `CcInfo` provider as its own.

This provider is an implementation detail not meant to be used by clients.
""",
    fields = {
        "cc_info": "The underlying `CcInfo` provider.",
        "objc_info": "The underlying `apple_common.Objc` provider.",
    },
)

def compile_swift_protos_for_target(
        *,
        additional_compiler_deps,
        additional_swift_proto_compiler_info,
        attr,
        ctx,
        module_name,
        proto_infos,
        swift_proto_compilers,
        swift_proto_deps,
        target_label):
    """ Compiles the Swift source files into a module.

    Args:
        additional_swift_proto_compiler_info: Dictionary of additional information passed to the Swift proto compiler.
        additional_compiler_deps: Additional dependencies passed directly to the Swift compiler.
        attr: The attributes of the target for which the module is being compiled.
        ctx: The context of the aspect or rule.
        module_name: The name of the Swift module that should be compiled from the protos.
        proto_infos: List of `ProtoInfo` providers to compile into Swift source files.
        swift_proto_compilers: List of targets propagating `SwiftProtoCompiler` providers.
        swift_proto_deps: List of targets propagating `SwiftProtoInfo` providers.
        target_label: The label of the target for which the module is being compiled.

    Returns:
        A struct with the following fields:
        direct_output_group_info: OutputGroupInfo provider generated directly by this target.
        direct_swift_proto_cc_info: SwiftProtoCcInfo provider generated directly by this target.
        direct_swift_info: SwiftInfo provider generated directly by this target.
        direct_swift_proto_info: SwiftProtoInfo provider generated directly by this target.
    """

    # Create a map of bundled proto paths for faster lookup:
    bundled_proto_paths = {}
    for swift_proto_compiler_target in swift_proto_compilers:
        swift_proto_compiler_info = swift_proto_compiler_target[SwiftProtoCompilerInfo]
        compiler_bundled_proto_paths = getattr(swift_proto_compiler_info, "bundled_proto_paths", [])
        for bundled_proto_path in compiler_bundled_proto_paths:
            bundled_proto_paths[bundled_proto_path] = True

    # Generate the module mappings:
    module_mappings = _generate_module_mappings(
        bundled_proto_paths = bundled_proto_paths,
        module_name = module_name,
        proto_infos = proto_infos,
        transitive_swift_proto_deps = swift_proto_deps,
    )

    # Use the proto compiler to compile the swift sources for the proto deps:
    compiler_deps = swift_proto_deps + additional_compiler_deps
    generated_swift_srcs = []
    for swift_proto_compiler_target in swift_proto_compilers:
        swift_proto_compiler_info = swift_proto_compiler_target[SwiftProtoCompilerInfo]
        compiler_deps.extend(swift_proto_compiler_info.compiler_deps)
        generated_swift_srcs.extend(swift_proto_compiler_info.compile(
            label = ctx.label,
            actions = ctx.actions,
            swift_proto_compiler_info = swift_proto_compiler_info,
            additional_compiler_info = additional_swift_proto_compiler_info,
            proto_infos = proto_infos,
            module_mappings = module_mappings,
        ))

    # Extract the swift toolchain and configure the features:
    swift_toolchain = swift_common.get_toolchain(ctx)
    feature_configuration = swift_common.configure_features(
        ctx = ctx,
        requested_features = ctx.features,
        swift_toolchain = swift_toolchain,
        unsupported_features = ctx.disabled_features,
    )

    # Compile the generated Swift source files as a module:
    include_dev_srch_paths = include_developer_search_paths(attr)
    compile_result = swift_common.compile(
        actions = ctx.actions,
        cc_infos = get_providers(compiler_deps, CcInfo),
        copts = ["-parse-as-library"] + getattr(attr, "copts", []),
        feature_configuration = feature_configuration,
        include_dev_srch_paths = include_dev_srch_paths,
        module_name = module_name,
        objc_infos = get_providers(compiler_deps, apple_common.Objc),
        package_name = getattr(attr, "package_name", None),
        srcs = generated_swift_srcs,
        swift_toolchain = swift_toolchain,
        swift_infos = get_providers(compiler_deps, SwiftInfo),
        target_name = target_label.name,
        workspace_name = ctx.workspace_name,
    )

    module_context = compile_result.module_context
    compilation_outputs = compile_result.compilation_outputs
    supplemental_outputs = compile_result.supplemental_outputs

    # Create the linking context from the compilation outputs:
    linking_context, linking_output = (
        swift_common.create_linking_context_from_compilation_outputs(
            actions = ctx.actions,
            compilation_outputs = compilation_outputs,
            feature_configuration = feature_configuration,
            include_dev_srch_paths = include_dev_srch_paths,
            label = target_label,
            linking_contexts = [
                dep[CcInfo].linking_context
                for dep in compiler_deps
                if CcInfo in dep
            ],
            module_context = module_context,
            swift_toolchain = swift_toolchain,
        )
    )

    # Extract the swift toolchain and configure the features:
    swift_toolchain = swift_common.get_toolchain(ctx)
    feature_configuration = swift_common.configure_features(
        ctx = ctx,
        requested_features = ctx.features,
        swift_toolchain = swift_toolchain,
        unsupported_features = ctx.disabled_features,
    )

    # Gather the transitive cc info providers:
    transitive_cc_infos = get_providers(
        compiler_deps,
        SwiftProtoCcInfo,
        lambda proto_cc_info: proto_cc_info.cc_info,
    ) + get_providers(compiler_deps, CcInfo)

    # Gather the transitive objc info providers:
    transitive_objc_infos = get_providers(
        compiler_deps,
        SwiftProtoCcInfo,
        lambda proto_cc_info: proto_cc_info.objc_info,
    ) + get_providers(compiler_deps, apple_common.Objc)

    # Gather the transitive swift info providers:
    transitive_swift_infos = get_providers(
        compiler_deps,
        SwiftInfo,
    )

    # Gather the transitive swift proto info providers:
    transitive_swift_proto_infos = get_providers(
        compiler_deps,
        SwiftProtoInfo,
    )

    # Create the direct cc info provider:
    direct_cc_info = cc_common.merge_cc_infos(
        direct_cc_infos = [
            CcInfo(
                compilation_context = module_context.clang.compilation_context,
                linking_context = linking_context,
            ),
        ],
        cc_infos = transitive_cc_infos,
    )

    # Create the direct objc info provider:
    direct_objc_info = new_objc_provider(
        additional_objc_infos = (
            transitive_objc_infos +
            swift_toolchain.implicit_deps_providers.objc_infos
        ),
        deps = [],
        feature_configuration = feature_configuration,
        is_test = False,
        module_context = module_context,
        libraries_to_link = [linking_output.library_to_link],
        swift_toolchain = swift_toolchain,
    )

    # Create the direct swift info provider:
    direct_swift_info = SwiftInfo(
        modules = [module_context],
        swift_infos = transitive_swift_infos,
    )

    # Create the direct output group info provider:
    direct_output_group_info = OutputGroupInfo(
        **supplemental_compilation_output_groups(
            supplemental_outputs,
        )
    )

    # Create the direct swift proto cc info provider:
    direct_swift_proto_cc_info = SwiftProtoCcInfo(
        cc_info = direct_cc_info,
        objc_info = direct_objc_info,
    )

    # Create the direct swift proto info:
    direct_swift_proto_info = SwiftProtoInfo(
        module_name = module_context.name,
        module_mappings = module_mappings,
        direct_pbswift_files = generated_swift_srcs,
        pbswift_files = depset(
            direct = generated_swift_srcs,
            transitive = [swift_proto_info.pbswift_files for swift_proto_info in transitive_swift_proto_infos],
        ),
    )

    return struct(
        direct_output_group_info = direct_output_group_info,
        direct_swift_info = direct_swift_info,
        direct_swift_proto_cc_info = direct_swift_proto_cc_info,
        direct_swift_proto_info = direct_swift_proto_info,
    )
