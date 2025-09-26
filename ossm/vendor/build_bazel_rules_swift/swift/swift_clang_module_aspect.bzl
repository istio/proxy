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

"""Propagates unified `SwiftInfo` providers for C/Objective-C targets."""

load("@bazel_skylib//lib:sets.bzl", "sets")
load("//swift/internal:attrs.bzl", "swift_toolchain_attrs")
load("//swift/internal:compiling.bzl", "precompile_clang_module")
load(
    "//swift/internal:feature_names.bzl",
    "SWIFT_FEATURE_MODULE_MAP_HOME_IS_CWD",
    "SWIFT_FEATURE_MODULE_MAP_NO_PRIVATE_HEADERS",
)
load(
    "//swift/internal:features.bzl",
    "configure_features",
    "is_feature_enabled",
)
load("//swift/internal:module_maps.bzl", "write_module_map")
load(
    "//swift/internal:swift_interop_info.bzl",
    "SwiftInteropInfo",
)
load(
    "//swift/internal:toolchain_utils.bzl",
    "get_swift_toolchain",
    "use_swift_toolchain",
)
load(
    "//swift/internal:utils.bzl",
    "compilation_context_for_explicit_module_compilation",
)
load(":module_name.bzl", "derive_swift_module_name")
load(
    ":providers.bzl",
    "SwiftClangModuleAspectInfo",
    "SwiftInfo",
    "create_clang_module_inputs",
    "create_swift_module_context",
)

_MULTIPLE_TARGET_ASPECT_ATTRS = [
    "deps",
]

def _compute_all_excluded_headers(*, exclude_headers, target):
    """Returns the full set of headers to exclude for a target.

    This function specifically handles the `cc_library` logic around the
    `include_prefix` and `strip_include_prefix` attributes, which cause Bazel to
    create a virtual header (symlink) for every public header in the target. For
    the generated module map to be created, we must exclude both the actual
    header file and the symlink.

    Args:
        exclude_headers: A list of `File`s representing headers that should be
            excluded from the module.
        target: The target to which the aspect is being applied.

    Returns:
        A list containing the complete set of headers that should be excluded,
        including any virtual header symlinks that match a real header in the
        excluded headers list passed into the function.
    """
    exclude_headers_set = sets.make(exclude_headers)
    virtual_exclude_headers = []

    for action in target.actions:
        if action.mnemonic != "Symlink":
            continue

        original_header = action.inputs.to_list()[0]
        virtual_header = action.outputs.to_list()[0]

        if sets.contains(exclude_headers_set, original_header):
            virtual_exclude_headers.append(virtual_header)

    return exclude_headers + virtual_exclude_headers

def _generate_module_map(
        *,
        actions,
        aspect_ctx,
        compilation_context,
        dependent_module_names,
        exclude_headers,
        feature_configuration,
        module_name,
        target):
    """Generates the module map file for the given target.

    Args:
        actions: The object used to register actions.
        aspect_ctx: The aspect context.
        compilation_context: The C++ compilation context that provides the
            headers for the module.
        dependent_module_names: A `list` of names of Clang modules that are
            direct dependencies of the target whose module map is being written.
        exclude_headers: A `list` of `File`s representing header files to
            exclude, if any, if we are generating the module map.
        feature_configuration: A Swift feature configuration.
        module_name: The name of the module.
        target: The target for which the module map is being generated.

    Returns: A `File` representing the generated module map.
    """

    # Determine if the toolchain requires module maps to use
    # workspace-relative paths or not, and other features controlling the
    # content permitted in the module map.
    workspace_relative = is_feature_enabled(
        feature_configuration = feature_configuration,
        feature_name = SWIFT_FEATURE_MODULE_MAP_HOME_IS_CWD,
    )
    exclude_private_headers = is_feature_enabled(
        feature_configuration = feature_configuration,
        feature_name = SWIFT_FEATURE_MODULE_MAP_NO_PRIVATE_HEADERS,
    )

    if exclude_private_headers:
        private_headers = []
    else:
        private_headers = compilation_context.direct_private_headers

    # Sort dependent module names and the headers to ensure a deterministic
    # order in the output file, in the event the compilation context would ever
    # change this on us. For files, use the execution path as the sorting key.
    def _path_sorting_key(file):
        return file.path

    # The headers in a `cc_inc_library` are actually symlinks to headers in its
    # `deps`. This interferes with layering because the `cc_inc_library` won't
    # depend directly on the libraries containing headers that the symlinked
    # headers include. Generating the module map with the symlinks as textual
    # headers instead of modular headers fixes this.
    if aspect_ctx.rule.kind == "cc_inc_library":
        public_headers = []
        textual_headers = sorted(
            compilation_context.direct_public_headers,
            key = _path_sorting_key,
        )
    else:
        public_headers = sorted(
            compilation_context.direct_public_headers,
            key = _path_sorting_key,
        )
        textual_headers = sorted(
            compilation_context.direct_textual_headers,
            key = _path_sorting_key,
        )

    module_map_file = actions.declare_file(
        "{}_modulemap/_/module.modulemap".format(target.label.name),
    )

    if exclude_headers:
        # If we're excluding headers from the module map, make sure to pick up
        # any virtual header symlinks that might be created, for example, by a
        # `cc_library` using the `include_prefix` and/or `strip_include_prefix`
        # attributes.
        exclude_headers = _compute_all_excluded_headers(
            exclude_headers = exclude_headers,
            target = target,
        )

    write_module_map(
        actions = actions,
        dependent_module_names = sorted(dependent_module_names),
        exclude_headers = sorted(exclude_headers, key = _path_sorting_key),
        exported_module_ids = ["*"],
        module_map_file = module_map_file,
        module_name = module_name,
        private_headers = sorted(private_headers, key = _path_sorting_key),
        public_headers = public_headers,
        public_textual_headers = textual_headers,
        workspace_relative = workspace_relative,
    )
    return module_map_file

def _objc_library_module_info(aspect_ctx):
    """Returns the `module_name` and `module_map` attrs for an `objc_library`.

    Args:
        aspect_ctx: The aspect context.

    Returns:
        A tuple containing the module name (a string) and the module map file (a
        `File`) specified as attributes on the `objc_library`. These values may
        be `None`.
    """
    attr = aspect_ctx.rule.attr

    # TODO(b/195019413): Deprecate the use of these attributes and use
    # `swift_interop_hint` to customize `objc_*` targets' module names and
    # module maps.
    module_name = getattr(attr, "module_name", None)
    module_map_file = None

    module_map_target = getattr(attr, "module_map", None)
    if module_map_target:
        module_map_files = module_map_target.files.to_list()
        if module_map_files:
            module_map_file = module_map_files[0]

    return module_name, module_map_file

def _module_info_for_target(
        target,
        aspect_ctx,
        compilation_context,
        dependent_module_names,
        exclude_headers,
        feature_configuration,
        module_name):
    """Returns the module name and module map for the target.

    Args:
        target: The target for which the module map is being generated.
        aspect_ctx: The aspect context.
        compilation_context: The C++ compilation context that provides the
            headers for the module.
        dependent_module_names: A `list` of names of Clang modules that are
            direct dependencies of the target whose module map is being written.
        exclude_headers: A `list` of `File`s representing header files to
            exclude, if any, if we are generating the module map.
        feature_configuration: A Swift feature configuration.
        module_name: The module name to prefer (if we're generating a module map
            from `SwiftInteropInfo`), or None to derive it from other
            properties of the target.

    Returns:
        A tuple containing the module name (a string) and module map file (a
        `File`) for the target. One or both of these values may be `None`.
    """

    # If a target doesn't have any headers, then don't generate a module map for
    # it. Such modules define nothing and only waste space on the compilation
    # command line and add more work for the compiler.
    if not compilation_context or (
        not compilation_context.direct_headers and
        not compilation_context.direct_textual_headers
    ):
        return None, None

    module_map_file = None

    if not module_name:
        if apple_common.Objc not in target:
            return None, None

        if aspect_ctx.rule.kind == "objc_library":
            module_name, module_map_file = _objc_library_module_info(aspect_ctx)

        # If it was an `objc_library` without an explicit module name, or it
        # was some other `Objc`-providing target, derive the module name
        # now.
        if not module_name:
            module_name = derive_swift_module_name(target.label)

    # If we didn't get a module map above, generate it now.
    if not module_map_file:
        module_map_file = _generate_module_map(
            actions = aspect_ctx.actions,
            aspect_ctx = aspect_ctx,
            compilation_context = compilation_context,
            dependent_module_names = dependent_module_names,
            exclude_headers = exclude_headers,
            feature_configuration = feature_configuration,
            module_name = module_name,
            target = target,
        )
    return module_name, module_map_file

def _handle_module(
        aspect_ctx,
        exclude_headers,
        feature_configuration,
        module_map_file,
        module_name,
        direct_swift_infos,
        swift_infos,
        swift_toolchain,
        target):
    """Processes a C/Objective-C target that is a dependency of a Swift target.

    Args:
        aspect_ctx: The aspect's context.
        exclude_headers: A `list` of `File`s representing header files to
            exclude, if any, if we are generating the module map.
        feature_configuration: The current feature configuration.
        module_map_file: The `.modulemap` file that defines the module, or None
            if it should be inferred from other properties of the target (for
            legacy support).
        module_name: The name of the module, or None if it should be inferred
            from other properties of the target (for legacy support).
        direct_swift_infos: The `SwiftInfo` providers of the current target's
            dependencies, which should be merged into the `SwiftInfo` provider
            created and returned for this target.
        swift_infos: The `SwiftInfo` providers of the current target's
            dependencies, which should be merged into the `SwiftInfo` provider
            created and returned for this target.
        swift_toolchain: The Swift toolchain being used to build this target.
        target: The C++ target to which the aspect is currently being applied.

    Returns:
        A list of providers that should be returned by the aspect.
    """
    attr = aspect_ctx.rule.attr

    all_swift_infos = (
        direct_swift_infos + swift_infos + swift_toolchain.clang_implicit_deps_providers.swift_infos
    )

    if CcInfo in target:
        compilation_context = target[CcInfo].compilation_context
    else:
        compilation_context = None

    # Collect the names of Clang modules that the module being built directly
    # depends on.
    dependent_module_names = []
    for swift_info in all_swift_infos:
        for module in swift_info.direct_modules:
            if module.clang:
                dependent_module_names.append(module.name)

    # If we weren't passed a module map (i.e., from a `SwiftInteropInfo`
    # provider), infer it and the module name based on properties of the rule to
    # support legacy rules.
    if not module_map_file:
        module_name, module_map_file = _module_info_for_target(
            target = target,
            aspect_ctx = aspect_ctx,
            compilation_context = compilation_context,
            dependent_module_names = dependent_module_names,
            exclude_headers = exclude_headers,
            feature_configuration = feature_configuration,
            module_name = module_name,
        )

    if not module_map_file:
        if all_swift_infos:
            return [
                SwiftInfo(
                    direct_swift_infos = direct_swift_infos,
                    swift_infos = swift_infos,
                ),
            ]
        else:
            return []

    compilation_contexts_to_merge_for_compilation = [compilation_context]

    # Fold the `strict_includes` from `apple_common.Objc` into the Clang module
    # descriptor in `SwiftInfo` so that the `Objc` provider doesn't have to be
    # passed as a separate input to Swift build APIs.
    if apple_common.Objc in target:
        strict_includes = target[apple_common.Objc].strict_include
        compilation_contexts_to_merge_for_compilation.append(
            cc_common.create_compilation_context(includes = strict_includes),
        )
    else:
        strict_includes = None

    # For each dependency, prefer the information from the original `CcInfo` if
    # we have it. If we don't, use the `SwiftInfo`-wrapped compilation context
    # instead.
    additional_swift_infos = []
    for attr_name in _MULTIPLE_TARGET_ASPECT_ATTRS:
        for dep in getattr(attr, attr_name, []):
            if CcInfo in dep:
                compilation_contexts_to_merge_for_compilation.append(
                    dep[CcInfo].compilation_context,
                )
            elif SwiftInfo in dep:
                additional_swift_infos.append(dep[SwiftInfo])

    compilation_context_to_compile = (
        compilation_context_for_explicit_module_compilation(
            compilation_contexts = (
                compilation_contexts_to_merge_for_compilation
            ),
            swift_infos = additional_swift_infos,
        )
    )

    pcm_outputs = precompile_clang_module(
        actions = aspect_ctx.actions,
        cc_compilation_context = compilation_context_to_compile,
        feature_configuration = feature_configuration,
        module_map_file = module_map_file,
        module_name = module_name,
        swift_infos = swift_infos,
        swift_toolchain = swift_toolchain,
        target_name = target.label.name,
    )
    precompiled_module = getattr(pcm_outputs, "pcm_file", None)
    indexstore_directory = getattr(pcm_outputs, "indexstore_directory", None)

    providers = [
        SwiftInfo(
            modules = [
                create_swift_module_context(
                    name = module_name,
                    clang = create_clang_module_inputs(
                        compilation_context = compilation_context,
                        module_map = module_map_file,
                        precompiled_module = precompiled_module,
                        strict_includes = strict_includes,
                    ),
                ),
            ],
            direct_swift_infos = direct_swift_infos,
            swift_infos = swift_infos,
        ),
    ]

    if indexstore_directory:
        providers.append(
            OutputGroupInfo(indexstore = depset([indexstore_directory])),
        )

    return providers

def _collect_swift_infos_from_deps(aspect_ctx):
    """Collect `SwiftInfo` providers from dependencies.

    Args:
        aspect_ctx: The aspect's context.

    Returns:
        A tuple of lists of `SwiftInfo` providers from dependencies of the target to which
        the aspect was applied. The first list contains those from attributes that should be treated
        as direct, while the second list contains those from all other attributes.
    """
    direct_swift_infos = []
    swift_infos = []

    attr = aspect_ctx.rule.attr
    for attr_name in _MULTIPLE_TARGET_ASPECT_ATTRS:
        infos = [
            dep[SwiftInfo]
            for dep in getattr(attr, attr_name, [])
            if SwiftInfo in dep
        ]
        swift_infos.extend(infos)

    return direct_swift_infos, swift_infos

def _find_swift_interop_info(target, aspect_ctx):
    """Finds a `SwiftInteropInfo` provider associated with the target.

    This function first looks at the target itself to determine if it propagated
    a `SwiftInteropInfo` provider directly (that is, its rule implementation
    function called `create_swift_interop_info`). If it did not, then the
    target's `aspect_hints` attribute is checked for a reference to a target
    that propagates `SwiftInteropInfo` (such as `swift_interop_hint`).

    It is an error if `aspect_hints` contains two or more targets that propagate
    `SwiftInteropInfo`, or if the target directly propagates the provider and
    there is also any target in `aspect_hints` that propagates it.

    Args:
        target: The target to which the aspect is currently being applied.
        aspect_ctx: The aspect's context.

    Returns:
        A tuple containing two elements:

        -   The `SwiftInteropInfo` associated with the target, if found;
            otherwise, None.
        -   A list of additional `SwiftInfo` providers that are treated as
            direct dependencies of the target, determined by reading attributes
            from the target if it did not provide `SwiftInteropInfo` directly.
    """
    if SwiftInteropInfo in target:
        # If the target's rule implementation directly provides
        # `SwiftInteropInfo`, then it is that rule's responsibility to collect
        # and merge `SwiftInfo` providers from relevant dependencies.
        interop_target = target
        interop_from_rule = True
        default_direct_swift_infos = []
        default_swift_infos = []
    else:
        # If the target's rule implementation does not directly provide
        # `SwiftInteropInfo`, then we need to collect the `SwiftInfo` providers
        # from the default dependencies and returns those. Note that if a custom
        # rule is used as a hint and returns a `SwiftInteropInfo` that contains
        # `SwiftInfo` providers, then we would consider the providers from the
        # default dependencies and the providers from the hint; they are merged
        # after the call site of this function.
        interop_target = None
        interop_from_rule = False
        default_direct_swift_infos, default_swift_infos = _collect_swift_infos_from_deps(aspect_ctx)

    # We don't break this loop early when we find a matching hint, because we
    # want to give an error message if there are two aspect hints that provide
    # `SwiftInteropInfo` (or if both the rule and an aspect hint do).
    # TODO: remove usage of `getattr` and use `aspect_ctx.rule.attr.aspect_hints` directly when we drop Bazel 6.
    for hint in getattr(aspect_ctx.rule.attr, "aspect_hints", []):
        if SwiftInteropInfo in hint:
            if interop_target:
                if interop_from_rule:
                    fail(("Conflicting Swift interop info from the target " +
                          "'{target}' ({rule} rule) and the aspect hint " +
                          "'{hint}'. Only one is allowed.").format(
                        hint = str(hint.label),
                        target = str(target.label),
                        rule = aspect_ctx.rule.kind,
                    ))
                else:
                    fail(("Conflicting Swift interop info from aspect hints " +
                          "'{hint1}' and '{hint2}'. Only one is " +
                          "allowed.").format(
                        hint1 = str(interop_target.label),
                        hint2 = str(hint.label),
                    ))
            interop_target = hint

    if interop_target:
        return interop_target[SwiftInteropInfo], default_direct_swift_infos, default_swift_infos
    return None, default_direct_swift_infos, default_swift_infos

def _swift_clang_module_aspect_impl(target, aspect_ctx):
    providers = [SwiftClangModuleAspectInfo()]

    # Do nothing if the target already propagates `SwiftInfo`.
    if SwiftInfo in target:
        return providers

    requested_features = aspect_ctx.features
    unsupported_features = aspect_ctx.disabled_features

    interop_info, direct_swift_infos, swift_infos = _find_swift_interop_info(target, aspect_ctx)
    if interop_info:
        # If the module should be suppressed, return immediately and propagate
        # nothing (not even transitive dependencies).
        if interop_info.suppressed:
            return providers

        exclude_headers = interop_info.exclude_headers
        module_map_file = interop_info.module_map
        module_name = (
            interop_info.module_name or derive_swift_module_name(target.label)
        )
        swift_infos.extend(interop_info.swift_infos)
        requested_features.extend(interop_info.requested_features)
        unsupported_features.extend(interop_info.unsupported_features)
    else:
        exclude_headers = []
        module_map_file = None
        module_name = None

    swift_toolchain = get_swift_toolchain(
        aspect_ctx,
        attr = "_toolchain_for_aspect",
    )
    feature_configuration = configure_features(
        ctx = aspect_ctx,
        requested_features = requested_features,
        swift_toolchain = swift_toolchain,
        unsupported_features = unsupported_features,
    )

    if interop_info or apple_common.Objc in target or CcInfo in target:
        return providers + _handle_module(
            aspect_ctx = aspect_ctx,
            exclude_headers = exclude_headers,
            feature_configuration = feature_configuration,
            module_map_file = module_map_file,
            module_name = module_name,
            direct_swift_infos = direct_swift_infos,
            swift_infos = swift_infos,
            swift_toolchain = swift_toolchain,
            target = target,
        )

    # If it's any other rule, just merge the `SwiftInfo` providers from its
    # deps.
    if direct_swift_infos or swift_infos:
        providers.append(SwiftInfo(
            direct_swift_infos = direct_swift_infos,
            swift_infos = swift_infos,
        ))

    return providers

swift_clang_module_aspect = aspect(
    attr_aspects = _MULTIPLE_TARGET_ASPECT_ATTRS,
    attrs = swift_toolchain_attrs(
        toolchain_attr_name = "_toolchain_for_aspect",
    ),
    doc = """\
Propagates unified `SwiftInfo` providers for targets that represent
C/Objective-C modules.

This aspect unifies the propagation of Clang module artifacts so that Swift
targets that depend on C/Objective-C targets can find the necessary module
artifacts, and so that Swift module artifacts are not lost when passing through
a non-Swift target in the build graph (for example, a `swift_library` that
depends on an `objc_library` that depends on a `swift_library`).

It also manages module map generation for targets that call
`create_swift_interop_info` and do not provide their own module map, and for
targets that use the `swift_interop_hint` aspect hint. Note that if one of these
approaches is used to interop with a target such as a `cc_library`, the headers
must be parsable as C, since Swift does not support C++ interop at this time.

Most users will not need to interact directly with this aspect, since it is
automatically applied to the `deps` attribute of all `swift_binary`,
`swift_library`, and `swift_test` targets. However, some rules may need to
provide custom propagation logic of C/Objective-C module dependencies; for
example, a rule that has a support library as a private attribute would need to
ensure that `SwiftInfo` providers for that library and its dependencies are
propagated to any targets that depend on it, since they would not be propagated
via `deps`. In this case, the custom rule can attach this aspect to that support
library's attribute and then merge its `SwiftInfo` provider with any others that
it propagates for its targets.

### Returned Providers

*   `SwiftClangModuleAspectInfo` _(always)_: An empty provider that is returned
    so that other aspects that want to depend on the outputs of this aspect can
    enforce ordering using `required_aspect_providers`.

*   `SwiftInfo` _(optional)_: This provider is returned when the aspect is
    applied to a target that is Swift-compatible or that has Swift-compatible
    transitive dependencies. It is _not_ returned when a target has its module
    suppressed (for example, using the `no_module` aspect hint). In this case,
    transitive dependency information is intentionally discarded.
""",
    fragments = ["cpp"],
    implementation = _swift_clang_module_aspect_impl,
    provides = [SwiftClangModuleAspectInfo],
    required_aspect_providers = [
        [apple_common.Objc],
        [CcInfo],
    ],
    toolchains = use_swift_toolchain(),
)
