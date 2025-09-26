# Copyright 2024 The Bazel Authors. All rights reserved.
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

"""Implementation of the `mixed_language_library` rule."""

load("@bazel_skylib//lib:dicts.bzl", "dicts")
load("@bazel_skylib//lib:paths.bzl", "paths")
load(
    "//swift:providers.bzl",
    "SwiftInfo",
    "create_clang_module_inputs",
    "create_swift_module_context",
)
load("//swift:swift_clang_module_aspect.bzl", "swift_clang_module_aspect")

# buildifier: disable=bzl-visibility
load("//swift/internal:attrs.bzl", "swift_deps_attr", "swift_toolchain_attrs")

# buildifier: disable=bzl-visibility
load(
    "//swift/internal:feature_names.bzl",
    "SWIFT_FEATURE_PROPAGATE_GENERATED_MODULE_MAP",
)

# buildifier: disable=bzl-visibility
load(
    "//swift/internal:features.bzl",
    "configure_features",
    "is_feature_enabled",
)

# buildifier: disable=bzl-visibility
load(
    "//swift/internal:toolchain_utils.bzl",
    "get_swift_toolchain",
    "use_swift_toolchain",
)

# buildifier: disable=bzl-visibility
load("//swift/internal:utils.bzl", "get_providers")

def _write_extended_module_map(
        *,
        actions,
        module_map_extender,
        module_name,
        original_module_map,
        output_module_map,
        swift_generated_header):
    args = actions.args()
    args.add(output_module_map)
    args.add(original_module_map)
    args.add(module_name)
    args.add(swift_generated_header.basename)

    actions.run(
        arguments = [args],
        executable = module_map_extender,
        inputs = [original_module_map],
        mnemonic = "ExtendMixedLanguageModuleMap",
        outputs = [output_module_map],
    )

def _mixed_language_library_impl(ctx):
    actions = ctx.actions
    clang_target = ctx.attr.clang_target
    module_name = ctx.attr.module_name
    name = ctx.attr.name

    swift_target = ctx.attr.swift_target
    swift_info = swift_target[SwiftInfo]

    feature_configuration = configure_features(
        ctx = ctx,
        requested_features = ctx.features,
        swift_toolchain = get_swift_toolchain(ctx),
        unsupported_features = ctx.disabled_features,
    )

    umbrella_header_symlink = actions.declare_file(
        "{name}/{module_name}/{module_name}.h".format(
            module_name = module_name,
            name = name,
        ),
    )
    ctx.actions.symlink(
        output = umbrella_header_symlink,
        target_file = ctx.file.umbrella_header,
    )

    swift_module = swift_info.direct_modules[0]
    swift_generated_header = swift_module.swift.generated_header
    if not swift_generated_header:
        fail("{} must have 'generate_header = True'".format(swift_target.label))

    swift_generated_header_symlink = actions.declare_file(
        "{name}/{module_name}/{basename}".format(
            module_name = module_name,
            name = name,
            basename = swift_generated_header.basename,
        ),
    )
    ctx.actions.symlink(
        output = swift_generated_header_symlink,
        target_file = swift_generated_header,
    )

    propagate_module_map = is_feature_enabled(
        feature_configuration = feature_configuration,
        feature_name = SWIFT_FEATURE_PROPAGATE_GENERATED_MODULE_MAP,
    )

    if propagate_module_map:
        module_map_basename = "module.modulemap"
    else:
        module_map_basename = name + ".modulemap"

    extended_module_map = actions.declare_file(
        "{name}/{module_name}/{basename}".format(
            basename = module_map_basename,
            module_name = module_name,
            name = name,
        ),
    )
    _write_extended_module_map(
        actions = actions,
        module_map_extender = ctx.executable._module_map_extender,
        module_name = module_name,
        original_module_map = ctx.file.module_map,
        output_module_map = extended_module_map,
        swift_generated_header = swift_generated_header_symlink,
    )

    outputs = [
        umbrella_header_symlink,
        extended_module_map,
        swift_generated_header_symlink,
    ]

    compilation_context = cc_common.create_compilation_context(
        headers = depset(outputs),
        direct_public_headers = outputs,
        includes = depset(
            # This allows `#import <ModuleName/ModuleName.h>` and
            # `#import <ModuleName/ModuleName-Swift.h>` to work
            [paths.dirname(umbrella_header_symlink.dirname)],
        ),
    )

    cc_info = cc_common.merge_cc_infos(
        direct_cc_infos = [
            CcInfo(compilation_context = compilation_context),
        ],
        cc_infos = [swift_target[CcInfo], clang_target[CcInfo]],
    )
    swift_info = SwiftInfo(
        modules = [
            create_swift_module_context(
                name = module_name,
                clang = create_clang_module_inputs(
                    compilation_context = cc_info.compilation_context,
                    module_map = extended_module_map,
                ),
                swift = swift_module.swift,
            ),
        ],
        # Collect transitive modules, without including `swift_target` (which is
        # covered with the `create_module` above)
        swift_infos = get_providers(ctx.attr.deps, SwiftInfo),
    )

    return [
        DefaultInfo(
            files = depset(
                outputs,
                transitive = [
                    swift_target[DefaultInfo].files,
                    clang_target[DefaultInfo].files,
                ],
            ),
            runfiles = ctx.runfiles(
                collect_data = True,
                collect_default = True,
                files = ctx.files.data,
            ),
        ),
        cc_info,
        coverage_common.instrumented_files_info(
            ctx,
            dependency_attributes = ["deps"],
        ),
        swift_info,
        # Propagate an `apple_common.Objc` provider with linking info about the
        # library so that linking with Apple Starlark APIs/rules works
        # correctly.
        # TODO(b/171413861): This can be removed when the Obj-C rules are
        # migrated to use `CcLinkingContext`.
        apple_common.new_objc_provider(
            providers = get_providers(
                [swift_target, clang_target],
                apple_common.Objc,
            ),
        ),
    ]

mixed_language_library = rule(
    attrs = dicts.add(
        swift_toolchain_attrs(),
        {
            "clang_target": attr.label(
                doc = """
The non-Swift portion of the mixed language module.
""",
                mandatory = True,
                providers = [CcInfo],
            ),
            "data": attr.label_list(
                allow_files = True,
                doc = """\
The list of files needed by this target at runtime.

Files and targets named in the `data` attribute will appear in the `*.runfiles`
area of this target, if it has one. This may include data files needed by a
binary or library, or other programs needed by it.
""",
            ),
            "deps": swift_deps_attr(
                aspects = [swift_clang_module_aspect],
                doc = "Dependencies of the target being built.",
            ),
            "swift_target": attr.label(
                doc = """
The Swift portion of the mixed language module.
""",
                mandatory = True,
                providers = [SwiftInfo],
            ),
            "umbrella_header": attr.label(
                allow_single_file = True,
                doc = "The umbrella header for the module.",
                mandatory = True,
            ),
            "module_name": attr.string(
                doc = "The name of the module.",
                mandatory = True,
            ),
            "module_map": attr.label(
                allow_single_file = True,
                doc = "The module map for the module.",
                mandatory = True,
            ),
            "_module_map_extender": attr.label(
                cfg = "exec",
                executable = True,
                default = Label("//tools/mixed_language_module_map_extender"),
            ),
        },
    ),
    doc = """\
Assembles a mixed language library from a clang and swift library target pair.
""",
    fragments = ["cpp"],
    implementation = _mixed_language_library_impl,
    toolchains = use_swift_toolchain(),
)
