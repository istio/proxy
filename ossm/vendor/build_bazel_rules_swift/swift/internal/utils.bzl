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

"""Common utility definitions used by various BUILD rules."""

load("@bazel_skylib//lib:paths.bzl", "paths")
load("//swift:providers.bzl", "SwiftInfo")

def collect_implicit_deps_providers(
        targets,
        additional_cc_infos = [],
        additional_objc_infos = []):
    """Returns a struct with important providers from a list of implicit deps.

    Note that the relationship between each provider in the list and the target
    it originated from is no longer retained.

    Args:
        targets: A list (possibly empty) of `Target`s.
        additional_cc_infos: A `list` of additional `CcInfo` providers that
            should be included in the returned value.
        additional_objc_infos: A `list` of additional `apple_common.Objc`
            providers that should be included in the returned value.

    Returns:
        A `struct` containing three fields:

        *   `cc_infos`: The merged `CcInfo` provider from the given targets.
        *   `objc_infos`: The merged `apple_common.Objc` provider from the given
            targets.
        *   `swift_infos`: The merged `SwiftInfo` provider from the given
            targets.
    """
    cc_infos = []
    objc_infos = []
    swift_infos = []

    for target in targets:
        if CcInfo in target:
            cc_infos.append(target[CcInfo])
        if apple_common.Objc in target:
            objc_infos.append(target[apple_common.Objc])
        if SwiftInfo in target:
            swift_infos.append(target[SwiftInfo])

    return struct(
        cc_infos = cc_infos + additional_cc_infos,
        objc_infos = objc_infos + additional_objc_infos,
        swift_infos = swift_infos,
    )

def compact(sequence):
    """Returns a copy of the sequence with any `None` items removed.

    Args:
        sequence: The sequence of items to compact.

    Returns: A copy of the sequence with any `None` items removed.
    """
    return [item for item in sequence if item != None]

def compilation_context_for_explicit_module_compilation(
        compilation_contexts,
        swift_infos):
    """Returns a compilation context suitable for compiling an explicit module.

    Args:
        compilation_contexts: `CcCompilationContext`s that provide information
            about headers and include paths for the target being compiled.
        swift_infos: `SwiftInfo` providers propagated by direct dependencies of
            the target being compiled.

    Returns:
        A `CcCompilationContext` containing information needed when compiling an
        explicit module, such as the headers and search paths of direct
        dependencies (since Clang needs to find those on the file system in
        order to map them to a module).
    """
    all_compilation_contexts = list(compilation_contexts)

    for swift_info in swift_infos:
        for module in swift_info.direct_modules:
            clang = module.clang
            if not clang:
                continue

            if clang.compilation_context:
                all_compilation_contexts.append(clang.compilation_context)
            if clang.strict_includes:
                all_compilation_contexts.append(
                    cc_common.create_compilation_context(
                        includes = clang.strict_includes,
                    ),
                )

    return merge_compilation_contexts(
        direct_compilation_contexts = all_compilation_contexts,
    )

def expand_locations(ctx, values, targets = []):
    """Expands the `$(location)` placeholders in each of the given values.

    Args:
        ctx: The rule context.
        values: A list of strings, which may contain `$(location)` placeholders.
        targets: A list of additional targets (other than the calling rule's
            `deps`) that should be searched for substitutable labels.

    Returns:
        A list of strings with any `$(location)` placeholders filled in.
    """
    return [ctx.expand_location(value, targets) for value in values]

def expand_make_variables(ctx, values, attribute_name):
    """Expands all references to Make variables in each of the given values.

    Args:
        ctx: The rule context.
        values: A list of strings, which may contain Make variable placeholders.
        attribute_name: The attribute name string that will be presented in
            console when an error occurs.

    Returns:
        A list of strings with Make variables placeholders filled in.
    """
    return [
        ctx.expand_make_variables(attribute_name, value, {})
        for value in values
    ]

def get_compilation_contexts(targets):
    """Returns the `CcCompilationContext` for each target in the given list.

    As with `get_providers`, it is not an error if a target in the list does not
    propagate `CcInfo`; those targets are simply ignored.

    Args:
        targets: A list of targets.

    Returns:
        Any `CcCompilationContext`s found in `CcInfo` providers among the
        targets in the list.
    """
    return get_providers(
        targets,
        CcInfo,
        lambda cc_info: cc_info.compilation_context,
    )

def get_swift_executable_for_toolchain(ctx):
    """Returns the Swift driver executable that the toolchain should use.

    Args:
        ctx: The toolchain's rule context.

    Returns:
        A `File` representing a custom Swift driver executable that the
        toolchain should use if provided by the toolchain target or by a command
        line option, or `None` if the default driver bundled with the toolchain
        should be used.
    """

    # If the toolchain target itself specifies a custom driver, use that.
    swift_executable = getattr(ctx.file, "swift_executable", None)

    # If no custom driver was provided by the target, check the value of the
    # command-line option and use that if it was provided.
    if not swift_executable:
        default_swift_executable_files = getattr(
            ctx.files,
            "_default_swift_executable",
            None,
        )

        if default_swift_executable_files:
            if len(default_swift_executable_files) > 1:
                fail(
                    "The 'default_swift_executable' option must point to a " +
                    "single file, but we found {}".format(
                        str(default_swift_executable_files),
                    ),
                )

            swift_executable = default_swift_executable_files[0]

    return swift_executable

def get_output_groups(targets, group_name):
    """Returns files in an output group from each target in a list.

    The returned list may not be the same size as `targets` if some of the
    targets do not contain the requested output group. This is not an error.

    Args:
        targets: A list of targets.
        group_name: The name of the output group.

    Returns:
        A list of `depset`s of `File`s from the requested output group for each
        target.
    """
    groups = []

    for target in targets:
        group = getattr(target[OutputGroupInfo], group_name, None)
        if group:
            groups.append(group)

    return groups

def get_providers(targets, provider, map_fn = None):
    """Returns the given provider (or a field) from each target in the list.

    The returned list may not be the same size as `targets` if some of the
    targets do not contain the requested provider. This is not an error.

    The main purpose of this function is to make this common operation more
    readable and prevent mistyping the list comprehension.

    Args:
        targets: A list of targets.
        provider: The provider to retrieve.
        map_fn: A function that takes a single argument and returns a single
            value. If this is present, it will be called on each provider in the
            list and the result will be returned in the list returned by
            `get_providers`.

    Returns:
        A list of the providers requested from the targets.
    """
    if map_fn:
        return [
            map_fn(target[provider])
            for target in targets
            if provider in target
        ]
    return [target[provider] for target in targets if provider in target]

def merge_compilation_contexts(
        direct_compilation_contexts = [],
        transitive_compilation_contexts = []):
    """Merges lists of direct and/or transitive compilation contexts.

    The `cc_common.merge_compilation_contexts` function only supports merging
    compilation contexts as direct contexts. To support merging contexts
    transitively, they must be wrapped in `CcInfo` providers. This helper
    function supports both cases, choosing the fast path (not wrapping) if no
    contexts are being merged transitively.

    Args:
        direct_compilation_contexts: A list of `CcCompilationContext`s whose
            direct fields (e.g., direct headers) should be preserved in the
            result.
        transitive_compilation_contexts: A list of `CcCompilationContext`s whose
            direct fields (e.g., direct headers) should not be preserved in the
            result. Headers in these providers will only be available in the
            transitive `headers` field.

    Returns:
        The merged `CcCompilationContext`.
    """
    if not transitive_compilation_contexts:
        # Fastest path: nothing to do but use the one direct.
        if len(direct_compilation_contexts) == 1:
            return direct_compilation_contexts[0]

        # Fast path: Everything can be merged with the direct API.
        return cc_common.merge_compilation_contexts(
            compilation_contexts = direct_compilation_contexts,
        )

    # Slow path: We must wrap each compilation context in a `CcInfo` provider to
    # get the correct direct vs. transitive behavior.
    return cc_common.merge_cc_infos(
        direct_cc_infos = [
            CcInfo(compilation_context = compilation_context)
            for compilation_context in direct_compilation_contexts
        ],
        cc_infos = [
            CcInfo(compilation_context = compilation_context)
            for compilation_context in transitive_compilation_contexts
        ],
    ).compilation_context

def merge_runfiles(all_runfiles):
    """Merges a list of `runfiles` objects.

    Args:
        all_runfiles: A list containing zero or more `runfiles` objects to
            merge.

    Returns:
        A merged `runfiles` object, or `None` if the list was empty.
    """
    result = None
    for runfiles in all_runfiles:
        if result == None:
            result = runfiles
        else:
            result = result.merge(runfiles)
    return result

def owner_relative_path(file):
    """Returns the part of the given file's path relative to its owning package.

    This function has extra logic to properly handle references to files in
    external repositoriies.

    Args:
        file: The file whose owner-relative path should be returned.

    Returns:
        The owner-relative path to the file.
    """
    root = file.owner.workspace_root
    package = file.owner.package

    if file.is_source:
        # Even though the docs say a File's `short_path` doesn't include the
        # root, Bazel special cases anything from an external repository and
        # includes a relative path (`../`) to the file. On the File's `owner` we
        # can get the `workspace_root` to try and line things up, but it is in
        # the form of "external/[name]". However the File's `path` does include
        # the root and leaves it in the "external/" form, so we just relativize
        # based on that instead.
        return paths.relativize(file.path, paths.join(root, package))
    elif root:
        # As above, but for generated files. The same mangling happens in
        # `short_path`, but since it is generated, the `path` includes the extra
        # output directories used by Bazel. So, we pick off the parent directory
        # segment that Bazel adds to the `short_path` and turn it into
        # "external/" so a relative path from the owner can be computed.
        short_path = file.short_path

        # Sanity check.
        if (
            not root.startswith("external/") or
            not short_path.startswith("../")
        ):
            fail(("Generated file in a different workspace with unexpected " +
                  "short_path ({short_path}) and owner.workspace_root " +
                  "({root}).").format(
                root = root,
                short_path = short_path,
            ))

        return paths.relativize(
            paths.join("external", short_path[3:]),
            paths.join(root, package),
        )
    else:
        return paths.relativize(file.short_path, package)

def struct_fields(s):
    """Returns a dictionary containing the fields in the struct `s`.

    Args:
        s: A `struct`.

    Returns:
        The fields in `s` and their values.
    """
    return {
        field: getattr(s, field)
        for field in dir(s)
        # TODO(b/36412967): Remove the `to_json` and `to_proto` checks.
        if field not in ("to_json", "to_proto")
    }

def include_developer_search_paths(attr):
    """Determines whether to include developer search paths.

    Args:
        attr: A rule's `ctx.attr`.

    Returns:
        A `bool` where `True` indicates that the developer search paths should
        be included during compilation. Otherwise, `False`.
    """
    return attr.testonly or getattr(
        attr,
        "always_include_developer_search_paths",
        False,
    )
