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

"""Implementation of linking logic for Swift."""

load("@bazel_skylib//lib:collections.bzl", "collections")
load("@bazel_skylib//lib:dicts.bzl", "dicts")
load(
    "//swift:swift_clang_module_aspect.bzl",
    "swift_clang_module_aspect",
)
load(":action_names.bzl", "SWIFT_ACTION_AUTOLINK_EXTRACT")
load(":actions.bzl", "is_action_enabled")
load(":attrs.bzl", "swift_compilation_attrs")
load(":autolinking.bzl", "register_autolink_extract_action")
load(
    ":debugging.bzl",
    "ensure_swiftmodule_is_embedded",
    "should_embed_swiftmodule_for_debugging",
)
load(
    ":developer_dirs.bzl",
    "developer_dirs_linkopts",
)
load(
    ":feature_names.bzl",
    "SWIFT_FEATURE_LLD_GC_WORKAROUND",
    "SWIFT_FEATURE_OBJC_LINK_FLAGS",
    "SWIFT_FEATURE__FORCE_ALWAYSLINK_TRUE",
)
load(
    ":features.bzl",
    "configure_features",
    "get_cc_feature_configuration",
    "is_feature_enabled",
)
load(":utils.bzl", "get_providers")

# TODO: Remove once we drop bazel 7.x
_OBJC_PROVIDER_LINKING = hasattr(apple_common.new_objc_provider(), "linkopt")

def binary_rule_attrs(
        *,
        additional_deps_aspects = [],
        additional_deps_providers = [],
        stamp_default):
    """Returns attributes common to both `swift_binary` and `swift_test`.

    Args:
        additional_deps_aspects: A list of additional aspects that should be
            applied to the `deps` attribute of the rule.
        additional_deps_providers: A list of lists representing additional
            providers that should be allowed by the `deps` attribute of the
            rule.
        stamp_default: The default value of the `stamp` attribute.

    Returns:
        A `dict` of attributes for a binary or test rule.
    """
    return dicts.add(
        swift_compilation_attrs(
            additional_deps_aspects = [
                swift_clang_module_aspect,
            ] + additional_deps_aspects,
            additional_deps_providers = additional_deps_providers,
            requires_srcs = False,
        ),
        {
            "linkopts": attr.string_list(
                doc = """\
Additional linker options that should be passed to `clang`. These strings are
subject to `$(location ...)` expansion.
""",
                mandatory = False,
            ),
            "malloc": attr.label(
                default = Label("@bazel_tools//tools/cpp:malloc"),
                doc = """\
Override the default dependency on `malloc`.

By default, Swift binaries are linked against `@bazel_tools//tools/cpp:malloc"`,
which is an empty library and the resulting binary will use libc's `malloc`.
This label must refer to a `cc_library` rule.
""",
                mandatory = False,
                providers = [[CcInfo]],
            ),
            "stamp": attr.int(
                default = stamp_default,
                doc = """\
Enable or disable link stamping; that is, whether to encode build information
into the binary. Possible values are:

* `stamp = 1`: Stamp the build information into the binary. Stamped binaries are
  only rebuilt when their dependencies change. Use this if there are tests that
  depend on the build information.

* `stamp = 0`: Always replace build information by constant values. This gives
  good build result caching.

* `stamp = -1`: Embedding of build information is controlled by the
  `--[no]stamp` flag.
""",
                mandatory = False,
            ),
            # Do not add references; temporary attribute for C++ toolchain
            # Starlark migration.
            "_cc_toolchain": attr.label(
                default = Label("@bazel_tools//tools/cpp:current_cc_toolchain"),
            ),
            # A late-bound attribute denoting the value of the `--custom_malloc`
            # command line flag (or None if the flag is not provided).
            "_custom_malloc": attr.label(
                default = configuration_field(
                    fragment = "cpp",
                    name = "custom_malloc",
                ),
                providers = [[CcInfo]],
            ),
        },
    )

def configure_features_for_binary(
        *,
        ctx,
        requested_features = [],
        swift_toolchain,
        unsupported_features = []):
    """Creates and returns the feature configuration for binary linking.

    This helper automatically handles common features for all Swift
    binary-creating targets, like code coverage.

    Args:
        ctx: The rule context.
        requested_features: Features that are requested for the target.
        swift_toolchain: The Swift toolchain provider.
        unsupported_features: Features that are unsupported for the target.

    Returns:
        The `FeatureConfiguration` that was created.
    """
    requested_features = list(requested_features)
    unsupported_features = list(unsupported_features)

    # Require static linking for now.
    requested_features.append("static_linking_mode")

    # Enable LLVM coverage in CROSSTOOL if this is a coverage build. Note that
    # we explicitly enable LLVM format and disable GCC format because the former
    # is the only one that Swift supports.
    if ctx.configuration.coverage_enabled:
        requested_features.append("llvm_coverage_map_format")
        unsupported_features.append("gcc_coverage_map_format")

    return configure_features(
        ctx = ctx,
        requested_features = requested_features,
        swift_toolchain = swift_toolchain,
        unsupported_features = unsupported_features,
    )

def _create_autolink_linking_context(
        *,
        actions,
        compilation_outputs,
        feature_configuration,
        label,
        name,
        swift_toolchain):
    """Creates a linking context that embeds a .swiftmodule for debugging.

    Args:
        actions: The context's `actions` object.
        compilation_outputs: A `CcCompilationOutputs` value containing the
            object files to link. Typically, this is the second tuple element in
            the value returned by `compile`.
        feature_configuration: A feature configuration obtained from
            `configure_features`.
        label: The `Label` of the target being built. This is used as the owner
            of the linker inputs created for post-compile actions (if any).
        name: The name of the target being linked, which is used to derive the
            output artifact.
        swift_toolchain: The `SwiftToolchainInfo` provider of the toolchain.

    Returns:
        A valid `CcLinkingContext`, or `None` if no linking context was created.
    """
    if compilation_outputs.objects and is_action_enabled(
        action_name = SWIFT_ACTION_AUTOLINK_EXTRACT,
        swift_toolchain = swift_toolchain,
    ):
        autolink_file = actions.declare_file(
            "{}.autolink".format(name),
        )
        register_autolink_extract_action(
            actions = actions,
            autolink_file = autolink_file,
            feature_configuration = feature_configuration,
            object_files = compilation_outputs.objects,
            swift_toolchain = swift_toolchain,
        )
        post_compile_linker_inputs = [
            cc_common.create_linker_input(
                owner = label,
                user_link_flags = depset(
                    ["@{}".format(autolink_file.path)],
                ),
                additional_inputs = depset([autolink_file]),
            ),
        ]
        return cc_common.create_linking_context(
            linker_inputs = depset(post_compile_linker_inputs),
        )

    return None

def _create_embedded_debugging_linking_context(
        *,
        actions,
        feature_configuration,
        label,
        module_context,
        swift_toolchain):
    """Creates a linking context that embeds a .swiftmodule for debugging.

    Args:
        actions: The context's `actions` object.
        feature_configuration: A feature configuration obtained from
            `configure_features`.
        label: The `Label` of the target being built. This is used as the owner
            of the linker inputs created for post-compile actions (if any).
        module_context: The module context returned by `compile`
            containing information about the Swift module that was compiled.
            Typically, this is the first tuple element in the value returned by
            `compile`.
        swift_toolchain: The `SwiftToolchainInfo` provider of the toolchain.

    Returns:
        A valid `CcLinkingContext`, or `None` if no linking context was created.
    """
    if (
        module_context and
        module_context.swift and
        should_embed_swiftmodule_for_debugging(
            feature_configuration = feature_configuration,
            module_context = module_context,
        )
    ):
        post_compile_linker_inputs = [
            ensure_swiftmodule_is_embedded(
                actions = actions,
                feature_configuration = feature_configuration,
                label = label,
                swiftmodule = module_context.swift.swiftmodule,
                swift_toolchain = swift_toolchain,
            ),
        ]
        return cc_common.create_linking_context(
            linker_inputs = depset(post_compile_linker_inputs),
        )

    return None

def create_linking_context_from_compilation_outputs(
        *,
        actions,
        additional_inputs = [],
        alwayslink = False,
        compilation_outputs,
        feature_configuration,
        is_test = None,
        include_dev_srch_paths = None,
        label,
        linking_contexts = [],
        module_context,
        name = None,
        swift_toolchain,
        user_link_flags = []):
    """Creates a linking context from the outputs of a Swift compilation.

    On some platforms, this function will spawn additional post-compile actions
    for the module in order to add their outputs to the linking context. For
    example, if the toolchain that requires a "module-wrap" invocation to embed
    the `.swiftmodule` into an object file for debugging purposes, or if it
    extracts auto-linking information from the object files to generate a linker
    command line parameters file, those actions will be created here.

    Args:
        actions: The context's `actions` object.
        additional_inputs: A `list` of `File`s containing any additional files
            that are referenced by `user_link_flags` and therefore need to be
            propagated up to the linker.
        alwayslink: If True, any binary that depends on the providers returned
            by this function will link in all of the library's object files,
            even if some contain no symbols referenced by the binary.
        compilation_outputs: A `CcCompilationOutputs` value containing the
            object files to link. Typically, this is the second tuple element in
            the value returned by `compile`.
        feature_configuration: A feature configuration obtained from
            `configure_features`.
        is_test: Deprecated. This argument will be removed in the next major
            release. Use the `include_dev_srch_paths` attribute instead.
            Represents if the `testonly` value of the context.
        include_dev_srch_paths: A `bool` that indicates whether the developer
            framework search paths will be added to the compilation command.
        label: The `Label` of the target being built. This is used as the owner
            of the linker inputs created for post-compile actions (if any), and
            the label's name component also determines the name of the artifact
            unless it is overridden by the `name` argument.
        linking_contexts: A `list` of `CcLinkingContext`s containing libraries
            from dependencies.
        name: A string that is used to derive the name of the library or
            libraries linked by this function. If this is not provided or is a
            falsy value, the name component of the `label` argument is used.
        module_context: The module context returned by `compile` containing
            information about the Swift module that was compiled. Typically,
            this is the first tuple element in the value returned by `compile`.
        swift_toolchain: The `SwiftToolchainInfo` provider of the toolchain.
        user_link_flags: A `list` of strings containing additional flags that
            will be passed to the linker for any binary that links with the
            returned linking context.

    Returns:
        A tuple of `(CcLinkingContext, CcLinkingOutputs)` containing the linking
        context to be propagated by the caller's `CcInfo` provider and the
        artifact representing the library that was linked, respectively.
    """
    extra_linking_contexts = [
        cc_info.linking_context
        for cc_info in swift_toolchain.implicit_deps_providers.cc_infos
    ]

    debugging_linking_context = _create_embedded_debugging_linking_context(
        actions = actions,
        feature_configuration = feature_configuration,
        label = label,
        module_context = module_context,
        swift_toolchain = swift_toolchain,
    )
    if debugging_linking_context:
        extra_linking_contexts.append(debugging_linking_context)

    if not name:
        name = label.name

    autolink_linking_context = _create_autolink_linking_context(
        actions = actions,
        compilation_outputs = compilation_outputs,
        feature_configuration = feature_configuration,
        label = label,
        name = name,
        swift_toolchain = swift_toolchain,
    )
    if autolink_linking_context:
        extra_linking_contexts.append(autolink_linking_context)

    if not alwayslink:
        alwayslink = is_feature_enabled(
            feature_configuration = feature_configuration,
            feature_name = SWIFT_FEATURE__FORCE_ALWAYSLINK_TRUE,
        )

    if is_feature_enabled(
        feature_configuration = feature_configuration,
        feature_name = SWIFT_FEATURE_LLD_GC_WORKAROUND,
    ):
        extra_linking_contexts.append(
            cc_common.create_linking_context(
                linker_inputs = depset([
                    cc_common.create_linker_input(
                        owner = label,
                        user_link_flags = depset(["-Wl,-z,nostart-stop-gc"]),
                    ),
                ]),
            ),
        )

    if is_feature_enabled(
        feature_configuration = feature_configuration,
        feature_name = SWIFT_FEATURE_OBJC_LINK_FLAGS,
    ):
        # TODO: Remove once we can rely on folks using the new toolchain
        extra_linking_contexts.append(
            cc_common.create_linking_context(
                linker_inputs = depset([
                    cc_common.create_linker_input(
                        owner = label,
                        user_link_flags = depset(["-ObjC"]),
                    ),
                ]),
            ),
        )

    if include_dev_srch_paths != None and is_test != None:
        fail("""\
Both `include_dev_srch_paths` and `is_test` cannot be specified. Please select \
one, preferring `include_dev_srch_paths`.\
""")
    include_dev_srch_paths_value = False
    if include_dev_srch_paths != None:
        include_dev_srch_paths_value = include_dev_srch_paths
    elif is_test != None:
        print("""\
WARNING: swift_common.create_linking_context_from_compilation_outputs(is_test \
= ...) is deprecated. Update your rules to use swift_common.\
create_linking_context_from_compilation_outputs(include_dev_srch_paths = ...) \
instead.\
""")  # buildifier: disable=print
        include_dev_srch_paths_value = is_test

    if include_dev_srch_paths_value:
        developer_paths_linkopts = developer_dirs_linkopts(swift_toolchain.developer_dirs)
    else:
        developer_paths_linkopts = []

    return cc_common.create_linking_context_from_compilation_outputs(
        actions = actions,
        feature_configuration = get_cc_feature_configuration(
            feature_configuration,
        ),
        cc_toolchain = swift_toolchain.cc_toolchain_info,
        compilation_outputs = compilation_outputs,
        name = name,
        user_link_flags = user_link_flags + developer_paths_linkopts,
        linking_contexts = linking_contexts + extra_linking_contexts,
        alwayslink = alwayslink,
        additional_inputs = additional_inputs,
        disallow_static_libraries = False,
        disallow_dynamic_library = True,
    )

def malloc_linking_context(ctx):
    """Returns the linking context to use for the malloc implementation.

    Args:
        ctx: The rule context.

    Returns:
        The `CcLinkingContext` that contains the library to link for the malloc
        implementation.
    """
    malloc = ctx.attr._custom_malloc or ctx.attr.malloc
    return malloc[CcInfo].linking_context

def new_objc_provider(
        *,
        additional_link_inputs = [],
        additional_objc_infos = [],
        alwayslink = False,
        deps,
        feature_configuration,
        is_test,
        libraries_to_link,
        module_context,
        user_link_flags = [],
        swift_toolchain):
    """Creates an `apple_common.Objc` provider for a Swift target.

    Args:
        additional_link_inputs: Additional linker input files that should be
            propagated to dependents.
        additional_objc_infos: Additional `apple_common.Objc` providers from
            transitive dependencies not provided by the `deps` argument.
        alwayslink: If True, any binary that depends on the providers returned
            by this function will link in all of the library's object files,
            even if some contain no symbols referenced by the binary.
        deps: The dependencies of the target being built, whose `Objc` providers
            will be passed to the new one in order to propagate the correct
            transitive fields.
        feature_configuration: The Swift feature configuration.
        is_test: Represents if the `testonly` value of the context.
        libraries_to_link: A list (typically of one element) of the
            `LibraryToLink` objects from which the static archives (`.a` files)
            containing the target's compiled code will be retrieved.
        module_context: The module context as returned by
            `compile`.
        user_link_flags: Linker options that should be propagated to dependents.
        swift_toolchain: The `SwiftToolchainInfo` provider of the toolchain.

    Returns:
        An `apple_common.Objc` provider that should be returned by the calling
        rule.
    """

    # The link action registered by `apple_common.link_multi_arch_binary` only
    # looks at `Objc` providers, not `CcInfo`, for libraries to link.
    # Dependencies from an `objc_library` to a `cc_library` are handled as a
    # special case, but other `cc_library` dependencies (such as `swift_library`
    # to `cc_library`) would be lost since they do not receive the same
    # treatment. Until those special cases are resolved via the unification of
    # the Obj-C and C++ rules, we need to collect libraries from `CcInfo` and
    # put them into the new `Objc` provider.
    transitive_cc_libs = []
    for cc_info in get_providers(deps, CcInfo):
        static_libs = []
        for linker_input in cc_info.linking_context.linker_inputs.to_list():
            for library_to_link in linker_input.libraries:
                library = library_to_link.static_library
                if library:
                    static_libs.append(library)
        transitive_cc_libs.append(depset(static_libs, order = "topological"))

    direct_libraries = []
    force_load_libraries = []

    for library_to_link in libraries_to_link:
        library = library_to_link.static_library
        if library:
            direct_libraries.append(library)
            if alwayslink:
                force_load_libraries.append(library)

    extra_linkopts = []
    if feature_configuration and should_embed_swiftmodule_for_debugging(
        feature_configuration = feature_configuration,
        module_context = module_context,
    ):
        module_file = module_context.swift.swiftmodule
        extra_linkopts.append("-Wl,-add_ast_path,{}".format(module_file.path))
        debug_link_inputs = [module_file]
    else:
        debug_link_inputs = []

    if is_test:
        extra_linkopts.extend(developer_dirs_linkopts(swift_toolchain.developer_dirs))

    if feature_configuration and is_feature_enabled(
        feature_configuration = feature_configuration,
        feature_name = SWIFT_FEATURE_OBJC_LINK_FLAGS,
    ):
        extra_linkopts.append("-ObjC")

    kwargs = {
        "providers": get_providers(
            deps,
            apple_common.Objc,
        ) + additional_objc_infos,
    }

    if _OBJC_PROVIDER_LINKING:
        kwargs = dicts.add(kwargs, {
            "force_load_library": depset(
                force_load_libraries,
                order = "topological",
            ),
            "library": depset(
                direct_libraries,
                transitive = transitive_cc_libs,
                order = "topological",
            ),
            "link_inputs": depset(additional_link_inputs + debug_link_inputs),
            "linkopt": depset(user_link_flags + extra_linkopts),
        })

    return apple_common.new_objc_provider(**kwargs)

def register_link_binary_action(
        *,
        actions,
        additional_inputs = [],
        additional_linking_contexts = [],
        additional_outputs = [],
        compilation_outputs,
        deps,
        feature_configuration,
        name,
        module_contexts = [],
        output_type,
        owner,
        stamp,
        swift_toolchain,
        user_link_flags = [],
        variables_extension = {}):
    """Registers an action that invokes the linker to produce a binary.

    Args:
        actions: The object used to register actions.
        additional_inputs: A list of additional inputs to the link action,
            such as those used in `$(location ...)` substitution, linker
            scripts, and so forth.
        additional_linking_contexts: Additional linking contexts that provide
            libraries or flags that should be linked into the executable.
        additional_outputs: Additional files that are outputs of the linking
            action but which are not among the return value of `cc_common.link`.
        compilation_outputs: A `CcCompilationOutputs` object containing object
            files that will be passed to the linker.
        deps: A list of targets representing additional libraries that will be
            passed to the linker.
        feature_configuration: The Swift feature configuration.
        module_contexts: A list of module contexts resulting from the
            compilation of the sources in the binary target, which are embedded
            in the binary for debugging if this is a debug build. This list may
            be empty if the target had no sources of its own.
        name: The name of the target being linked, which is used to derive the
            output artifact.
        output_type: A string indicating the output type; "executable" or
            "dynamic_library".
        owner: The `Label` of the target that owns this linker input.
        stamp: A tri-state value (-1, 0, or 1) that specifies whether link
            stamping is enabled. See `cc_common.link` for details about the
            behavior of this argument.
        swift_toolchain: The `SwiftToolchainInfo` provider of the toolchain.
        user_link_flags: Additional flags passed to the linker. Any
            `$(location ...)` placeholders are assumed to have already been
            expanded.
        variables_extension: A dictionary containing additional crosstool
            variables that should be set for the linking action.

    Returns:
        A `CcLinkingOutputs` object that contains the `executable` or
        `library_to_link` that was linked (depending on the value of the
        `output_type` argument).
    """
    linking_contexts = []

    for dep in deps:
        if CcInfo in dep:
            cc_info = dep[CcInfo]
            linking_contexts.append(cc_info.linking_context)

        # TODO(allevato): Remove all of this when `apple_common.Objc` goes away.
        if apple_common.Objc in dep:
            objc = dep[apple_common.Objc]

            def get_objc_list(objc, attr):
                return getattr(objc, attr, depset([])).to_list()

            # We don't need to handle the `objc.sdk_framework` field here
            # because those values have also been put into the user link flags
            # of a CcInfo, but the others don't seem to have been.
            dep_link_flags = [
                "-l{}".format(dylib)
                for dylib in get_objc_list(objc, "sdk_dylib")
            ]
            dep_link_flags.extend([
                "-F{}".format(path)
                for path in get_objc_list(objc, "dynamic_framework_paths")
            ])
            dep_link_flags.extend(collections.before_each(
                "-framework",
                get_objc_list(objc, "dynamic_framework_names"),
            ))
            dep_link_flags.extend([
                "-F{}".format(path)
                for path in get_objc_list(objc, "static_framework_paths")
            ])
            dep_link_flags.extend(collections.before_each(
                "-framework",
                get_objc_list(objc, "static_framework_names"),
            ))

            is_bazel_6 = hasattr(apple_common, "link_multi_arch_static_library")
            if not hasattr(objc, "static_framework_file"):
                additional_inputs = depset([])
            elif is_bazel_6:
                additional_inputs = objc.static_framework_file
            else:
                additional_inputs = depset(
                    transitive = [
                        objc.static_framework_file,
                        objc.imported_library,
                    ],
                )
                dep_link_flags.extend([
                    lib.path
                    for lib in objc.imported_library.to_list()
                ])

            linking_contexts.append(
                cc_common.create_linking_context(
                    linker_inputs = depset([
                        cc_common.create_linker_input(
                            owner = owner,
                            user_link_flags = dep_link_flags,
                            additional_inputs = additional_inputs,
                        ),
                    ]),
                ),
            )

    linking_contexts.extend(additional_linking_contexts)

    for module_context in module_contexts:
        debugging_linking_context = _create_embedded_debugging_linking_context(
            actions = actions,
            feature_configuration = feature_configuration,
            label = owner,
            module_context = module_context,
            swift_toolchain = swift_toolchain,
        )
        if debugging_linking_context:
            linking_contexts.append(debugging_linking_context)

    autolink_linking_context = _create_autolink_linking_context(
        actions = actions,
        compilation_outputs = compilation_outputs,
        feature_configuration = feature_configuration,
        label = owner,
        name = name,
        swift_toolchain = swift_toolchain,
    )
    if autolink_linking_context:
        linking_contexts.append(autolink_linking_context)

    if is_feature_enabled(
        feature_configuration = feature_configuration,
        feature_name = SWIFT_FEATURE_LLD_GC_WORKAROUND,
    ):
        linking_contexts.append(
            cc_common.create_linking_context(
                linker_inputs = depset([
                    cc_common.create_linker_input(
                        owner = owner,
                        user_link_flags = depset(["-Wl,-z,nostart-stop-gc"]),
                    ),
                ]),
            ),
        )

    if is_feature_enabled(
        feature_configuration = feature_configuration,
        feature_name = SWIFT_FEATURE_OBJC_LINK_FLAGS,
    ):
        # TODO: Remove once we can rely on folks using the new toolchain
        linking_contexts.append(
            cc_common.create_linking_context(
                linker_inputs = depset([
                    cc_common.create_linker_input(
                        owner = owner,
                        user_link_flags = depset(["-ObjC"]),
                    ),
                ]),
            ),
        )

    # Collect linking contexts from any of the toolchain's implicit
    # dependencies.
    linking_contexts.extend([
        cc_info.linking_context
        for cc_info in swift_toolchain.implicit_deps_providers.cc_infos
    ])

    return cc_common.link(
        actions = actions,
        additional_inputs = additional_inputs,
        additional_outputs = additional_outputs,
        cc_toolchain = swift_toolchain.cc_toolchain_info,
        compilation_outputs = compilation_outputs,
        feature_configuration = get_cc_feature_configuration(
            feature_configuration,
        ),
        name = name,
        user_link_flags = user_link_flags,
        linking_contexts = linking_contexts,
        link_deps_statically = True,
        output_type = output_type,
        stamp = stamp,
        variables_extension = variables_extension,
    )
