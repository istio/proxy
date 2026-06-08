"""Rust allocator library rules"""

load("@rules_cc//cc/common:cc_common.bzl", "cc_common")
load("@rules_cc//cc/common:cc_info.bzl", "CcInfo")
load(
    "//rust/private:utils.bzl",
    "dedent",
    "find_toolchain",
)
load(
    ":common.bzl",
    "rust_common",
)
load(
    ":providers.bzl",
    "AllocatorLibrariesImplInfo",
    "AllocatorLibrariesInfo",
)

def _ltl(library, actions, cc_toolchain, feature_configuration):
    """A helper to generate `LibraryToLink` objects

    Args:
        library (File): A rust library file to link.
        actions: The rule's ctx.actions object.
        cc_toolchain (CcToolchainInfo): A cc toolchain provider to be used (can be None).
        feature_configuration (feature_configuration): feature_configuration to be queried (can be None).

    Returns:
        LibraryToLink: A provider containing information about libraries to link.
    """
    return cc_common.create_library_to_link(
        actions = actions,
        feature_configuration = feature_configuration,
        cc_toolchain = cc_toolchain,
        static_library = library,
        pic_static_library = library,
    )

def make_libstd_and_allocator_ccinfo(
        *,
        cc_toolchain,
        feature_configuration,
        label,
        actions,
        experimental_link_std_dylib,
        rust_std,
        allocator_library,
        std = "std"):
    """Make the CcInfo (if possible) for libstd and allocator libraries.

    Args:
        cc_toolchain (CcToolchainInfo): A cc toolchain provider to be used.
        feature_configuration (feature_configuration): feature_configuration to be queried.
        label (Label): The rule's label.
        actions: The rule's ctx.actions object.
        experimental_link_std_dylib (boolean): The value of the standard library's `_experimental_link_std_dylib(ctx)`.
        rust_std: The Rust standard library.
        allocator_library (struct): The target to use for providing allocator functions.
          This should be a struct with either:
          * a cc_info field of type CcInfo
          * an allocator_libraries_impl_info field, which should be None or of type AllocatorLibrariesImplInfo.
        std: Standard library flavor. Currently only "std" and "no_std_with_alloc" are supported,
             accompanied with the default panic behavior.


    Returns:
        A CcInfo object for the required libraries, or None if no such libraries are available.
    """
    cc_infos = []
    if not type(allocator_library) == "struct":
        fail("Unexpected type of allocator_library, it must be a struct.")
    if not any([hasattr(allocator_library, field) for field in ["cc_info", "allocator_libraries_impl_info"]]):
        fail("Unexpected contents of allocator_library, it must provide either a cc_info or an allocator_libraries_impl_info.")

    if not rust_common.stdlib_info in rust_std:
        fail(dedent("""\
            {} --
            The `rust_lib` ({}) must be a target providing `rust_common.stdlib_info`
            (typically `rust_stdlib_filegroup` rule from @rules_rust//rust:defs.bzl).
            See https://github.com/bazelbuild/rules_rust/pull/802 for more information.
        """).format(label, rust_std))
    rust_stdlib_info = rust_std[rust_common.stdlib_info]

    if rust_stdlib_info.self_contained_files:
        compilation_outputs = cc_common.create_compilation_outputs(
            objects = depset(rust_stdlib_info.self_contained_files),
        )

        # Include C++ toolchain files as additional inputs for cross-compilation scenarios
        additional_inputs = []
        if cc_toolchain:
            if cc_toolchain.all_files:
                additional_inputs = cc_toolchain.all_files.to_list()

            linking_context, _linking_outputs = cc_common.create_linking_context_from_compilation_outputs(
                name = label.name,
                actions = actions,
                feature_configuration = feature_configuration,
                cc_toolchain = cc_toolchain,
                compilation_outputs = compilation_outputs,
                additional_inputs = additional_inputs,
            )

            cc_infos.append(CcInfo(
                linking_context = linking_context,
            ))

    if rust_stdlib_info.std_rlibs:
        allocator_library_inputs = []

        if hasattr(allocator_library, "allocator_libraries_impl_info") and allocator_library.allocator_libraries_impl_info:
            static_archive = allocator_library.allocator_libraries_impl_info.static_archive
            allocator_library_inputs = [depset(
                [_ltl(static_archive, actions, cc_toolchain, feature_configuration)],
            )]

        alloc_inputs = depset(
            [_ltl(f, actions, cc_toolchain, feature_configuration) for f in rust_stdlib_info.alloc_files],
            transitive = allocator_library_inputs,
            order = "topological",
        )
        between_alloc_and_core_inputs = depset(
            [_ltl(f, actions, cc_toolchain, feature_configuration) for f in rust_stdlib_info.between_alloc_and_core_files],
            transitive = [alloc_inputs],
            order = "topological",
        )
        core_inputs = depset(
            [_ltl(f, actions, cc_toolchain, feature_configuration) for f in rust_stdlib_info.core_files],
            transitive = [between_alloc_and_core_inputs],
            order = "topological",
        )

        # The libraries panic_abort and panic_unwind are alternatives.
        # The std by default requires panic_unwind.
        # Exclude panic_abort if panic_unwind is present.
        # TODO: Provide a setting to choose between panic_abort and panic_unwind.
        filtered_between_core_and_std_files = rust_stdlib_info.between_core_and_std_files
        has_panic_unwind = [
            f
            for f in filtered_between_core_and_std_files
            if "panic_unwind" in f.basename
        ]
        if has_panic_unwind:
            filtered_between_core_and_std_files = [
                f
                for f in filtered_between_core_and_std_files
                if "abort" not in f.basename
            ]
            core_alloc_and_panic_inputs = depset(
                [
                    _ltl(f, actions, cc_toolchain, feature_configuration)
                    for f in rust_stdlib_info.panic_files
                    if "unwind" not in f.basename
                ],
                transitive = [core_inputs],
                order = "topological",
            )
        else:
            core_alloc_and_panic_inputs = depset(
                [
                    _ltl(f, actions, cc_toolchain, feature_configuration)
                    for f in rust_stdlib_info.panic_files
                    if "unwind" not in f.basename
                ],
                transitive = [core_inputs],
                order = "topological",
            )
        memchr_inputs = depset(
            [
                _ltl(f, actions, cc_toolchain, feature_configuration)
                for f in rust_stdlib_info.memchr_files
            ],
            transitive = [core_inputs],
            order = "topological",
        )
        between_core_and_std_inputs = depset(
            [
                _ltl(f, actions, cc_toolchain, feature_configuration)
                for f in filtered_between_core_and_std_files
            ],
            transitive = [memchr_inputs],
            order = "topological",
        )

        if experimental_link_std_dylib:
            # std dylib has everything so that we do not need to include all std_files
            std_inputs = depset(
                [cc_common.create_library_to_link(
                    actions = actions,
                    feature_configuration = feature_configuration,
                    cc_toolchain = cc_toolchain,
                    dynamic_library = rust_stdlib_info.std_dylib,
                )],
            )
        else:
            std_inputs = depset(
                [
                    _ltl(f, actions, cc_toolchain, feature_configuration)
                    for f in rust_stdlib_info.std_files
                ],
                transitive = [between_core_and_std_inputs],
                order = "topological",
            )

        test_inputs = depset(
            [
                _ltl(f, actions, cc_toolchain, feature_configuration)
                for f in rust_stdlib_info.test_files
            ],
            transitive = [std_inputs],
            order = "topological",
        )

        if std == "std":
            link_inputs = cc_common.create_linker_input(
                owner = rust_std.label,
                libraries = test_inputs,
            )
        elif std == "no_std_with_alloc":
            link_inputs = cc_common.create_linker_input(
                owner = rust_std.label,
                libraries = core_alloc_and_panic_inputs,
            )
        else:
            fail("Requested '{}' std mode is currently not supported.".format(std))

        allocator_inputs = None
        if hasattr(allocator_library, "cc_info"):
            allocator_inputs = [allocator_library.cc_info.linking_context.linker_inputs]

        cc_infos.append(CcInfo(
            linking_context = cc_common.create_linking_context(
                linker_inputs = depset(
                    [link_inputs],
                    transitive = allocator_inputs,
                    order = "topological",
                ),
            ),
        ))

    if cc_infos:
        return cc_common.merge_cc_infos(
            direct_cc_infos = cc_infos,
        )
    return None

# Attributes for rust-based allocator library support.
# Can't add it directly to RUSTC_ATTRS above, as those are used as
# aspect parameters and only support simple types ('bool', 'int' or 'string').
RUSTC_ALLOCATOR_LIBRARIES_ATTRS = {
    # This is really internal. Not prefixed with `_` since we need to adapt this
    # in bootstrapping situations, e.g., when building the process wrapper
    # or allocator libraries themselves.
    "allocator_libraries": attr.label(
        default = Label("//ffi/rs:default_allocator_libraries"),
        providers = [AllocatorLibrariesInfo],
    ),
}

def _rust_allocator_libraries_impl(ctx):
    allocator_library = ctx.attr.allocator_library[AllocatorLibrariesImplInfo] if ctx.attr.allocator_library else None
    global_allocator_library = ctx.attr.global_allocator_library[AllocatorLibrariesImplInfo] if ctx.attr.global_allocator_library else None

    toolchain = find_toolchain(ctx)

    def make_cc_info(info, std):
        return toolchain.make_libstd_and_allocator_ccinfo(
            ctx.label,
            ctx.actions,
            struct(allocator_libraries_impl_info = info),
            std,
        )

    providers = [AllocatorLibrariesInfo(
        allocator_library = allocator_library,
        global_allocator_library = global_allocator_library,
        libstd_and_allocator_ccinfo = make_cc_info(allocator_library, "std"),
        libstd_and_global_allocator_ccinfo = make_cc_info(global_allocator_library, "std"),
        nostd_and_global_allocator_ccinfo = make_cc_info(global_allocator_library, "no_std_with_alloc"),
    )]

    return providers

rust_allocator_libraries = rule(
    implementation = _rust_allocator_libraries_impl,
    provides = [AllocatorLibrariesInfo],
    attrs = {
        "allocator_library": attr.label(
            doc = "An optional library to provide when a default rust allocator is used.",
            providers = [AllocatorLibrariesImplInfo],
        ),
        "global_allocator_library": attr.label(
            doc = "An optional library to provide when a default rust allocator is used.",
            providers = [AllocatorLibrariesImplInfo],
        ),
    },
    toolchains = [
        str(Label("//rust:toolchain_type")),
        config_common.toolchain_type("@bazel_tools//tools/cpp:toolchain_type", mandatory = False),
    ],
)
