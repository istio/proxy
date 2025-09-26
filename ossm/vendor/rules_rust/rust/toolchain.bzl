"""The rust_toolchain rule definition and implementation."""

load("@bazel_skylib//rules:common_settings.bzl", "BuildSettingInfo")
load("//rust/platform:triple.bzl", "triple")
load("//rust/private:common.bzl", "rust_common")
load("//rust/private:lto.bzl", "RustLtoInfo")
load("//rust/private:rust_analyzer.bzl", _rust_analyzer_toolchain = "rust_analyzer_toolchain")
load(
    "//rust/private:rustfmt.bzl",
    _current_rustfmt_toolchain = "current_rustfmt_toolchain",
    _rustfmt_toolchain = "rustfmt_toolchain",
)
load(
    "//rust/private:utils.bzl",
    "dedent",
    "dedup_expand_location",
    "find_cc_toolchain",
    "is_exec_configuration",
    "is_std_dylib",
    "make_static_lib_symlink",
)
load("//rust/settings:incompatible.bzl", "IncompatibleFlagInfo")

rust_analyzer_toolchain = _rust_analyzer_toolchain
rustfmt_toolchain = _rustfmt_toolchain
current_rustfmt_toolchain = _current_rustfmt_toolchain

def _rust_stdlib_filegroup_impl(ctx):
    rust_std = ctx.files.srcs
    dot_a_files = []
    between_alloc_and_core_files = []
    core_files = []
    between_core_and_std_files = []
    std_files = []
    test_files = []
    memchr_files = []
    alloc_files = []
    self_contained_files = [
        file
        for file in rust_std
        if file.basename.endswith(".o") and "self-contained" in file.path
    ]
    panic_files = []

    std_rlibs = [f for f in rust_std if f.basename.endswith(".rlib")]
    if std_rlibs:
        # test depends on std
        # std depends on everything except test
        #
        # core only depends on alloc, but we poke adler in there
        # because that needs to be before miniz_oxide
        #
        # panic_unwind depends on unwind, alloc, cfg_if, compiler_builtins, core, libc
        # panic_abort depends on alloc, cfg_if, compiler_builtins, core, libc
        #
        # alloc depends on the allocator_library if it's configured, but we
        # do that later.
        dot_a_files = [make_static_lib_symlink(ctx.label.package, ctx.actions, f) for f in std_rlibs]

        alloc_files = [f for f in dot_a_files if "alloc" in f.basename and "std" not in f.basename]
        between_alloc_and_core_files = [f for f in dot_a_files if "compiler_builtins" in f.basename]
        core_files = [f for f in dot_a_files if ("core" in f.basename or "adler" in f.basename) and "std" not in f.basename]
        panic_files = [f for f in dot_a_files if f.basename in ["cfg_if", "libc", "panic_abort", "panic_unwind", "unwind"]]
        between_core_and_std_files = [
            f
            for f in dot_a_files
            if "alloc" not in f.basename and "compiler_builtins" not in f.basename and "core" not in f.basename and "adler" not in f.basename and "std" not in f.basename and "memchr" not in f.basename and "test" not in f.basename
        ]
        memchr_files = [f for f in dot_a_files if "memchr" in f.basename]
        std_files = [f for f in dot_a_files if "std" in f.basename]
        test_files = [f for f in dot_a_files if "test" in f.basename]

        partitioned_files_len = len(alloc_files) + len(between_alloc_and_core_files) + len(core_files) + len(between_core_and_std_files) + len(memchr_files) + len(std_files) + len(test_files)
        if partitioned_files_len != len(dot_a_files):
            partitioned = alloc_files + between_alloc_and_core_files + core_files + between_core_and_std_files + memchr_files + std_files + test_files
            for f in sorted(partitioned):
                # buildifier: disable=print
                print("File partitioned: {}".format(f.basename))
            fail("rust_toolchain couldn't properly partition rlibs in rust_std. Partitioned {} out of {} files. This is probably a bug in the rule implementation.".format(partitioned_files_len, len(dot_a_files)))

    std_dylib = None

    for file in rust_std:
        if is_std_dylib(file):
            std_dylib = file
            break

    return [
        DefaultInfo(
            files = depset(ctx.files.srcs),
            runfiles = ctx.runfiles(ctx.files.srcs),
        ),
        rust_common.stdlib_info(
            std_rlibs = std_rlibs,
            dot_a_files = dot_a_files,
            between_alloc_and_core_files = between_alloc_and_core_files,
            core_files = core_files,
            between_core_and_std_files = between_core_and_std_files,
            std_files = std_files,
            std_dylib = std_dylib,
            test_files = test_files,
            memchr_files = memchr_files,
            alloc_files = alloc_files,
            self_contained_files = self_contained_files,
            panic_files = panic_files,
            srcs = ctx.attr.srcs,
        ),
    ]

rust_stdlib_filegroup = rule(
    doc = "A dedicated filegroup-like rule for Rust stdlib artifacts.",
    implementation = _rust_stdlib_filegroup_impl,
    attrs = {
        "srcs": attr.label_list(
            allow_files = True,
            doc = "The list of targets/files that are components of the rust-stdlib file group",
            mandatory = True,
        ),
    },
)

def _ltl(library, ctx, cc_toolchain, feature_configuration):
    """A helper to generate `LibraryToLink` objects

    Args:
        library (File): A rust library file to link.
        ctx (ctx): The rule's context object.
        cc_toolchain (CcToolchainInfo): A cc toolchain provider to be used.
        feature_configuration (feature_configuration): feature_configuration to be queried.

    Returns:
        LibraryToLink: A provider containing information about libraries to link.
    """
    return cc_common.create_library_to_link(
        actions = ctx.actions,
        feature_configuration = feature_configuration,
        cc_toolchain = cc_toolchain,
        static_library = library,
        pic_static_library = library,
    )

def _make_libstd_and_allocator_ccinfo(ctx, rust_std, allocator_library, std = "std"):
    """Make the CcInfo (if possible) for libstd and allocator libraries.

    Args:
        ctx (ctx): The rule's context object.
        rust_std: The Rust standard library.
        allocator_library: The target to use for providing allocator functions.
        std: Standard library flavor. Currently only "std" and "no_std_with_alloc" are supported,
             accompanied with the default panic behavior.


    Returns:
        A CcInfo object for the required libraries, or None if no such libraries are available.
    """
    cc_toolchain, feature_configuration = find_cc_toolchain(ctx)
    cc_infos = []

    if not rust_common.stdlib_info in rust_std:
        fail(dedent("""\
            {} --
            The `rust_lib` ({}) must be a target providing `rust_common.stdlib_info`
            (typically `rust_stdlib_filegroup` rule from @rules_rust//rust:defs.bzl).
            See https://github.com/bazelbuild/rules_rust/pull/802 for more information.
        """).format(ctx.label, rust_std))
    rust_stdlib_info = rust_std[rust_common.stdlib_info]

    if rust_stdlib_info.self_contained_files:
        compilation_outputs = cc_common.create_compilation_outputs(
            objects = depset(rust_stdlib_info.self_contained_files),
        )

        linking_context, _linking_outputs = cc_common.create_linking_context_from_compilation_outputs(
            name = ctx.label.name,
            actions = ctx.actions,
            feature_configuration = feature_configuration,
            cc_toolchain = cc_toolchain,
            compilation_outputs = compilation_outputs,
        )

        cc_infos.append(CcInfo(
            linking_context = linking_context,
        ))

    if rust_stdlib_info.std_rlibs:
        alloc_inputs = depset(
            [_ltl(f, ctx, cc_toolchain, feature_configuration) for f in rust_stdlib_info.alloc_files],
        )
        between_alloc_and_core_inputs = depset(
            [_ltl(f, ctx, cc_toolchain, feature_configuration) for f in rust_stdlib_info.between_alloc_and_core_files],
            transitive = [alloc_inputs],
            order = "topological",
        )
        core_inputs = depset(
            [_ltl(f, ctx, cc_toolchain, feature_configuration) for f in rust_stdlib_info.core_files],
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
                    _ltl(f, ctx, cc_toolchain, feature_configuration)
                    for f in rust_stdlib_info.panic_files
                    if "unwind" not in f.basename
                ],
                transitive = [core_inputs],
                order = "topological",
            )
        else:
            core_alloc_and_panic_inputs = depset(
                [
                    _ltl(f, ctx, cc_toolchain, feature_configuration)
                    for f in rust_stdlib_info.panic_files
                    if "unwind" not in f.basename
                ],
                transitive = [core_inputs],
                order = "topological",
            )
        memchr_inputs = depset(
            [
                _ltl(f, ctx, cc_toolchain, feature_configuration)
                for f in rust_stdlib_info.memchr_files
            ],
            transitive = [core_inputs],
            order = "topological",
        )
        between_core_and_std_inputs = depset(
            [
                _ltl(f, ctx, cc_toolchain, feature_configuration)
                for f in filtered_between_core_and_std_files
            ],
            transitive = [memchr_inputs],
            order = "topological",
        )

        if _experimental_link_std_dylib(ctx):
            # std dylib has everything so that we do not need to include all std_files
            std_inputs = depset(
                [cc_common.create_library_to_link(
                    actions = ctx.actions,
                    feature_configuration = feature_configuration,
                    cc_toolchain = cc_toolchain,
                    dynamic_library = rust_stdlib_info.std_dylib,
                )],
            )
        else:
            std_inputs = depset(
                [
                    _ltl(f, ctx, cc_toolchain, feature_configuration)
                    for f in rust_stdlib_info.std_files
                ],
                transitive = [between_core_and_std_inputs],
                order = "topological",
            )

        test_inputs = depset(
            [
                _ltl(f, ctx, cc_toolchain, feature_configuration)
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
        if allocator_library:
            allocator_inputs = [allocator_library[CcInfo].linking_context.linker_inputs]

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

def _symlink_sysroot_tree(ctx, name, target):
    """Generate a set of symlinks to files from another target

    Args:
        ctx (ctx): The toolchain's context object
        name (str): The name of the sysroot directory (typically `ctx.label.name`)
        target (Target): A target owning files to symlink

    Returns:
        depset[File]: A depset of the generated symlink files
    """
    tree_files = []
    for file in target.files.to_list():
        # Parse the path to the file relative to the workspace root so a
        # symlink matching this path can be created within the sysroot.

        # The code blow attempts to parse any workspace names out of the
        # path. For local targets, this code is a noop.
        if target.label.workspace_root:
            file_path = file.path.split(target.label.workspace_root, 1)[-1]
        else:
            file_path = file.path

        symlink = ctx.actions.declare_file("{}/{}".format(name, file_path.lstrip("/")))

        ctx.actions.symlink(
            output = symlink,
            target_file = file,
        )

        tree_files.append(symlink)

    return depset(tree_files)

def _symlink_sysroot_bin(ctx, name, directory, target):
    """Crete a symlink to a target file.

    Args:
        ctx (ctx): The rule's context object
        name (str): A common name for the output directory
        directory (str): The directory under `name` to put the file in
        target (File): A File object to symlink to

    Returns:
        File: A newly generated symlink file
    """
    symlink = ctx.actions.declare_file("{}/{}/{}".format(
        name,
        directory,
        target.basename,
    ))

    ctx.actions.symlink(
        output = symlink,
        target_file = target,
        is_executable = True,
    )

    return symlink

def _generate_sysroot(
        ctx,
        rustc,
        rustdoc,
        rustc_lib,
        cargo = None,
        clippy = None,
        cargo_clippy = None,
        llvm_tools = None,
        rust_std = None,
        rustfmt = None):
    """Generate a rust sysroot from collection of toolchain components

    Args:
        ctx (ctx): A context object from a `rust_toolchain` rule.
        rustc (File): The path to a `rustc` executable.
        rustdoc (File): The path to a `rustdoc` executable.
        rustc_lib (Target): A collection of Files containing dependencies of `rustc`.
        cargo (File, optional): The path to a `cargo` executable.
        cargo_clippy (File, optional): The path to a `cargo-clippy` executable.
        clippy (File, optional): The path to a `clippy-driver` executable.
        llvm_tools (Target, optional): A collection of llvm tools used by `rustc`.
        rust_std (Target, optional): A collection of Files containing Rust standard library components.
        rustfmt (File, optional): The path to a `rustfmt` executable.

    Returns:
        struct: A struct of generated files representing the new sysroot
    """
    name = ctx.label.name

    # Define runfiles
    direct_files = []
    transitive_file_sets = []

    # Rustc
    sysroot_rustc = _symlink_sysroot_bin(ctx, name, "bin", rustc)
    direct_files.extend([sysroot_rustc])

    # Rustc dependencies
    sysroot_rustc_lib = None
    if rustc_lib:
        sysroot_rustc_lib = _symlink_sysroot_tree(ctx, name, rustc_lib)
        transitive_file_sets.extend([sysroot_rustc_lib])

    # Rustdoc
    sysroot_rustdoc = _symlink_sysroot_bin(ctx, name, "bin", rustdoc)
    direct_files.extend([sysroot_rustdoc])

    # Clippy
    sysroot_clippy = None
    if clippy:
        sysroot_clippy = _symlink_sysroot_bin(ctx, name, "bin", clippy)
        direct_files.extend([sysroot_clippy])

    # Cargo
    sysroot_cargo = None
    if cargo:
        sysroot_cargo = _symlink_sysroot_bin(ctx, name, "bin", cargo)
        direct_files.extend([sysroot_cargo])

    # Cargo-clippy
    sysroot_cargo_clippy = None
    if cargo_clippy:
        sysroot_cargo_clippy = _symlink_sysroot_bin(ctx, name, "bin", cargo_clippy)
        direct_files.extend([sysroot_cargo_clippy])

    # Rustfmt
    sysroot_rustfmt = None
    if rustfmt:
        sysroot_rustfmt = _symlink_sysroot_bin(ctx, name, "bin", rustfmt)
        direct_files.extend([sysroot_rustfmt])

    # Llvm tools
    sysroot_llvm_tools = None
    if llvm_tools:
        sysroot_llvm_tools = _symlink_sysroot_tree(ctx, name, llvm_tools)
        transitive_file_sets.extend([sysroot_llvm_tools])

    # Rust standard library
    sysroot_rust_std = None
    if rust_std:
        sysroot_rust_std = _symlink_sysroot_tree(ctx, name, rust_std)
        transitive_file_sets.extend([sysroot_rust_std])

        # Made available to support $(location) expansion in stdlib_linkflags and extra_rustc_flags.
        transitive_file_sets.append(depset(ctx.files.rust_std))

    # Declare a file in the root of the sysroot to make locating the sysroot easy
    sysroot_anchor = ctx.actions.declare_file("{}/rust.sysroot".format(name))
    ctx.actions.write(
        output = sysroot_anchor,
        content = "\n".join([
            "cargo: {}".format(cargo),
            "clippy: {}".format(clippy),
            "cargo-clippy: {}".format(cargo_clippy),
            "llvm_tools: {}".format(llvm_tools),
            "rust_std: {}".format(rust_std),
            "rustc_lib: {}".format(rustc_lib),
            "rustc: {}".format(rustc),
            "rustdoc: {}".format(rustdoc),
            "rustfmt: {}".format(rustfmt),
        ]),
    )

    # Create a depset of all sysroot files (symlinks and their real paths)
    all_files = depset(direct_files, transitive = transitive_file_sets)

    return struct(
        all_files = all_files,
        cargo = sysroot_cargo,
        clippy = sysroot_clippy,
        cargo_clippy = sysroot_cargo_clippy,
        rust_std = sysroot_rust_std,
        rustc = sysroot_rustc,
        rustc_lib = sysroot_rustc_lib,
        rustdoc = sysroot_rustdoc,
        rustfmt = sysroot_rustfmt,
        sysroot_anchor = sysroot_anchor,
    )

def _experimental_use_cc_common_link(ctx):
    return ctx.attr.experimental_use_cc_common_link[BuildSettingInfo].value

def _rust_toolchain_impl(ctx):
    """The rust_toolchain implementation

    Args:
        ctx (ctx): The rule's context object

    Returns:
        list: A list containing the target's toolchain Provider info
    """
    compilation_mode_opts = {}
    for k, opt_level in ctx.attr.opt_level.items():
        if not k in ctx.attr.debug_info:
            fail("Compilation mode {} is not defined in debug_info but is defined opt_level".format(k))
        if not k in ctx.attr.strip_level:
            fail("Compilation mode {} is not defined in strip_level but is defined opt_level".format(k))
        compilation_mode_opts[k] = struct(debug_info = ctx.attr.debug_info[k], opt_level = opt_level, strip_level = ctx.attr.strip_level[k])
    for k in ctx.attr.debug_info.keys():
        if not k in ctx.attr.opt_level:
            fail("Compilation mode {} is not defined in opt_level but is defined debug_info".format(k))

    rename_first_party_crates = ctx.attr._rename_first_party_crates[BuildSettingInfo].value
    third_party_dir = ctx.attr._third_party_dir[BuildSettingInfo].value
    pipelined_compilation = ctx.attr._pipelined_compilation[BuildSettingInfo].value
    no_std = ctx.attr._no_std[BuildSettingInfo].value
    lto = ctx.attr._lto[RustLtoInfo]

    experimental_use_global_allocator = ctx.attr._experimental_use_global_allocator[BuildSettingInfo].value
    if _experimental_use_cc_common_link(ctx):
        if experimental_use_global_allocator and not ctx.attr.global_allocator_library:
            fail("rust_toolchain.experimental_use_cc_common_link with --@rules_rust//rust/settings:experimental_use_global_allocator " +
                 "requires rust_toolchain.global_allocator_library to be set")
        if not ctx.attr.allocator_library:
            fail("rust_toolchain.experimental_use_cc_common_link requires rust_toolchain.allocator_library to be set")
    if experimental_use_global_allocator and not _experimental_use_cc_common_link(ctx):
        fail(
            "Using @rules_rust//rust/settings:experimental_use_global_allocator requires" +
            "--@rules_rust//rust/settings:experimental_use_cc_common_link to be set",
        )

    rust_std = ctx.attr.rust_std

    sysroot = _generate_sysroot(
        ctx = ctx,
        rustc = ctx.file.rustc,
        rustdoc = ctx.file.rust_doc,
        rustc_lib = ctx.attr.rustc_lib,
        rust_std = rust_std,
        rustfmt = ctx.file.rustfmt,
        clippy = ctx.file.clippy_driver,
        cargo = ctx.file.cargo,
        cargo_clippy = ctx.file.cargo_clippy,
        llvm_tools = ctx.attr.llvm_tools,
    )

    expanded_stdlib_linkflags = []
    for flag in ctx.attr.stdlib_linkflags:
        expanded_stdlib_linkflags.append(
            dedup_expand_location(
                ctx,
                flag,
                targets = rust_std[rust_common.stdlib_info].srcs,
            ),
        )

    expanded_extra_rustc_flags = []
    for flag in ctx.attr.extra_rustc_flags:
        expanded_extra_rustc_flags.append(
            dedup_expand_location(
                ctx,
                flag,
                targets = rust_std[rust_common.stdlib_info].srcs,
            ),
        )

    linking_context = cc_common.create_linking_context(
        linker_inputs = depset([
            cc_common.create_linker_input(
                owner = ctx.label,
                user_link_flags = depset(expanded_stdlib_linkflags),
            ),
        ]),
    )

    # Contains linker flags needed to link Rust standard library.
    # These need to be added to linker command lines when the linker is not rustc
    # (rustc does this automatically). Linker flags wrapped in an otherwise empty
    # `CcInfo` to provide the flags in a way that doesn't duplicate them per target
    # providing a `CcInfo`.
    stdlib_linkflags_cc_info = CcInfo(
        compilation_context = cc_common.create_compilation_context(),
        linking_context = linking_context,
    )

    # Determine the path and short_path of the sysroot
    sysroot_path = sysroot.sysroot_anchor.dirname
    sysroot_short_path, _, _ = sysroot.sysroot_anchor.short_path.rpartition("/")

    # Variables for make variable expansion
    make_variables = {
        "RUSTC": sysroot.rustc.path,
        "RUSTDOC": sysroot.rustdoc.path,
        "RUST_DEFAULT_EDITION": ctx.attr.default_edition or "",
        "RUST_SYSROOT": sysroot_path,
        "RUST_SYSROOT_SHORT": sysroot_short_path,
    }

    if sysroot.cargo:
        make_variables.update({
            "CARGO": sysroot.cargo.path,
        })

    if sysroot.rustfmt:
        make_variables.update({
            "RUSTFMT": sysroot.rustfmt.path,
        })

    make_variable_info = platform_common.TemplateVariableInfo(make_variables)

    exec_triple = triple(ctx.attr.exec_triple)

    if not exec_triple.system:
        fail("No system was provided for the execution platform. Please update {}".format(
            ctx.label,
        ))

    if ctx.attr.target_triple and ctx.attr.target_json:
        fail("Do not specify both target_triple and target_json, either use a builtin triple or provide a custom specification file. Please update {}".format(
            ctx.label,
        ))

    target_triple = None
    target_json = None
    target_arch = None
    target_os = None

    if ctx.attr.target_triple:
        target_triple = triple(ctx.attr.target_triple)
        target_arch = target_triple.arch
        target_os = target_triple.system

    elif ctx.attr.target_json:
        # Ensure the data provided is valid json
        target_json_content = json.decode(ctx.attr.target_json)
        target_json = ctx.actions.declare_file("{}.target.json".format(ctx.label.name))

        ctx.actions.write(
            output = target_json,
            content = json.encode_indent(target_json_content, indent = " " * 4),
        )

        if "arch" in target_json_content:
            target_arch = target_json_content["arch"]
        if "os" in target_json_content:
            target_os = target_json_content["os"]
    else:
        fail("Either `target_triple` or `target_json` must be provided. Please update {}".format(
            ctx.label,
        ))

    toolchain = platform_common.ToolchainInfo(
        all_files = sysroot.all_files,
        binary_ext = ctx.attr.binary_ext,
        cargo = sysroot.cargo,
        clippy_driver = sysroot.clippy,
        cargo_clippy = sysroot.cargo_clippy,
        compilation_mode_opts = compilation_mode_opts,
        crosstool_files = ctx.files._cc_toolchain,
        default_edition = ctx.attr.default_edition,
        dylib_ext = ctx.attr.dylib_ext,
        env = ctx.attr.env,
        exec_triple = exec_triple,
        libstd_and_allocator_ccinfo = _make_libstd_and_allocator_ccinfo(ctx, rust_std, ctx.attr.allocator_library, "std"),
        libstd_and_global_allocator_ccinfo = _make_libstd_and_allocator_ccinfo(ctx, rust_std, ctx.attr.global_allocator_library, "std"),
        nostd_and_global_allocator_cc_info = _make_libstd_and_allocator_ccinfo(ctx, rust_std, ctx.attr.global_allocator_library, "no_std_with_alloc"),
        llvm_cov = ctx.file.llvm_cov,
        llvm_profdata = ctx.file.llvm_profdata,
        make_variables = make_variable_info,
        rust_doc = sysroot.rustdoc,
        rust_std = sysroot.rust_std,
        rust_std_paths = depset([file.dirname for file in sysroot.rust_std.to_list()]),
        rustc = sysroot.rustc,
        rustc_lib = sysroot.rustc_lib,
        rustfmt = sysroot.rustfmt,
        staticlib_ext = ctx.attr.staticlib_ext,
        stdlib_linkflags = stdlib_linkflags_cc_info,
        extra_rustc_flags = expanded_extra_rustc_flags,
        extra_rustc_flags_for_crate_types = ctx.attr.extra_rustc_flags_for_crate_types,
        extra_exec_rustc_flags = ctx.attr.extra_exec_rustc_flags,
        per_crate_rustc_flags = ctx.attr.per_crate_rustc_flags,
        sysroot = sysroot_path,
        sysroot_short_path = sysroot_short_path,
        target_arch = target_arch,
        target_flag_value = target_json.path if target_json else target_triple.str,
        target_json = target_json,
        target_os = target_os,
        target_triple = target_triple,

        # Experimental and incompatible flags
        _rename_first_party_crates = rename_first_party_crates,
        _third_party_dir = third_party_dir,
        _pipelined_compilation = pipelined_compilation,
        _experimental_link_std_dylib = _experimental_link_std_dylib(ctx),
        _experimental_use_cc_common_link = _experimental_use_cc_common_link(ctx),
        _experimental_use_global_allocator = experimental_use_global_allocator,
        _experimental_use_coverage_metadata_files = ctx.attr._experimental_use_coverage_metadata_files[BuildSettingInfo].value,
        _incompatible_change_rust_test_compilation_output_directory = ctx.attr._incompatible_change_rust_test_compilation_output_directory[IncompatibleFlagInfo].enabled,
        _toolchain_generated_sysroot = ctx.attr._toolchain_generated_sysroot[BuildSettingInfo].value,
        _incompatible_do_not_include_data_in_compile_data = ctx.attr._incompatible_do_not_include_data_in_compile_data[IncompatibleFlagInfo].enabled,
        _no_std = no_std,
        _lto = lto,
    )
    return [
        toolchain,
        make_variable_info,
    ]

def _experimental_link_std_dylib(ctx):
    return not is_exec_configuration(ctx) and \
           ctx.attr.experimental_link_std_dylib[BuildSettingInfo].value and \
           ctx.attr.rust_std[rust_common.stdlib_info].std_dylib != None

rust_toolchain = rule(
    implementation = _rust_toolchain_impl,
    fragments = ["cpp"],
    attrs = {
        "allocator_library": attr.label(
            doc = "Target that provides allocator functions when rust_library targets are embedded in a cc_binary.",
            default = "@rules_rust//ffi/cc/allocator_library",
        ),
        "binary_ext": attr.string(
            doc = "The extension for binaries created from rustc.",
            mandatory = True,
        ),
        "cargo": attr.label(
            doc = "The location of the `cargo` binary. Can be a direct source or a filegroup containing one item.",
            allow_single_file = True,
            cfg = "exec",
        ),
        "cargo_clippy": attr.label(
            doc = "The location of the `cargo_clippy` binary. Can be a direct source or a filegroup containing one item.",
            allow_single_file = True,
            cfg = "exec",
        ),
        "clippy_driver": attr.label(
            doc = "The location of the `clippy-driver` binary. Can be a direct source or a filegroup containing one item.",
            allow_single_file = True,
            cfg = "exec",
        ),
        "debug_info": attr.string_dict(
            doc = "Rustc debug info levels per opt level",
            default = {
                "dbg": "2",
                "fastbuild": "0",
                "opt": "0",
            },
        ),
        "default_edition": attr.string(
            doc = (
                "The edition to use for rust_* rules that don't specify an edition. " +
                "If absent, every rule is required to specify its `edition` attribute."
            ),
        ),
        "dylib_ext": attr.string(
            doc = "The extension for dynamic libraries created from rustc.",
            mandatory = True,
        ),
        "env": attr.string_dict(
            doc = "Environment variables to set in actions.",
        ),
        "exec_triple": attr.string(
            doc = (
                "The platform triple for the toolchains execution environment. " +
                "For more details see: https://docs.bazel.build/versions/master/skylark/rules.html#configurations"
            ),
            mandatory = True,
        ),
        "experimental_link_std_dylib": attr.label(
            default = Label("@rules_rust//rust/settings:experimental_link_std_dylib"),
            doc = "Label to a boolean build setting that controls whether whether to link libstd dynamically.",
        ),
        "experimental_use_cc_common_link": attr.label(
            default = Label("//rust/settings:experimental_use_cc_common_link"),
            doc = "Label to a boolean build setting that controls whether cc_common.link is used to link rust binaries.",
        ),
        "extra_exec_rustc_flags": attr.string_list(
            doc = "Extra flags to pass to rustc in exec configuration",
        ),
        "extra_rustc_flags": attr.string_list(
            doc = "Extra flags to pass to rustc in non-exec configuration. Subject to location expansion with respect to the srcs of the `rust_std` attribute.",
        ),
        "extra_rustc_flags_for_crate_types": attr.string_list_dict(
            doc = "Extra flags to pass to rustc based on crate type",
        ),
        "global_allocator_library": attr.label(
            doc = "Target that provides allocator functions for when a global allocator is present.",
            default = "@rules_rust//ffi/cc/global_allocator_library",
        ),
        "llvm_cov": attr.label(
            doc = "The location of the `llvm-cov` binary. Can be a direct source or a filegroup containing one item. If None, rust code is not instrumented for coverage.",
            allow_single_file = True,
            cfg = "exec",
        ),
        "llvm_profdata": attr.label(
            doc = "The location of the `llvm-profdata` binary. Can be a direct source or a filegroup containing one item. If `llvm_cov` is None, this can be None as well and rust code is not instrumented for coverage.",
            allow_single_file = True,
            cfg = "exec",
        ),
        "llvm_tools": attr.label(
            doc = "LLVM tools that are shipped with the Rust toolchain.",
            allow_files = True,
        ),
        "opt_level": attr.string_dict(
            doc = "Rustc optimization levels.",
            default = {
                "dbg": "0",
                "fastbuild": "0",
                "opt": "3",
            },
        ),
        "per_crate_rustc_flags": attr.string_list(
            doc = "Extra flags to pass to rustc in non-exec configuration",
        ),
        "rust_doc": attr.label(
            doc = "The location of the `rustdoc` binary. Can be a direct source or a filegroup containing one item.",
            allow_single_file = True,
            cfg = "exec",
            mandatory = True,
        ),
        "rust_std": attr.label(
            doc = "The Rust standard library.",
            mandatory = True,
        ),
        "rustc": attr.label(
            doc = "The location of the `rustc` binary. Can be a direct source or a filegroup containing one item.",
            allow_single_file = True,
            cfg = "exec",
            mandatory = True,
        ),
        "rustc_lib": attr.label(
            doc = "The libraries used by rustc during compilation.",
            cfg = "exec",
        ),
        "rustfmt": attr.label(
            doc = "**Deprecated**: Instead see [rustfmt_toolchain](#rustfmt_toolchain)",
            allow_single_file = True,
            cfg = "exec",
        ),
        "staticlib_ext": attr.string(
            doc = "The extension for static libraries created from rustc.",
            mandatory = True,
        ),
        "stdlib_linkflags": attr.string_list(
            doc = (
                "Additional linker flags to use when Rust standard library is linked by a C++ linker " +
                "(rustc will deal with these automatically). Subject to location expansion with respect " +
                "to the srcs of the `rust_std` attribute."
            ),
            mandatory = True,
        ),
        "strip_level": attr.string_dict(
            doc = (
                "Rustc strip levels. For all potential options, see " +
                "https://doc.rust-lang.org/rustc/codegen-options/index.html#strip"
            ),
            default = {
                "dbg": "none",
                "fastbuild": "none",
                "opt": "debuginfo",
            },
        ),
        "target_json": attr.string(
            doc = ("Override the target_triple with a custom target specification. " +
                   "For more details see: https://doc.rust-lang.org/rustc/targets/custom.html"),
        ),
        "target_triple": attr.string(
            doc = (
                "The platform triple for the toolchains target environment. " +
                "For more details see: https://docs.bazel.build/versions/master/skylark/rules.html#configurations"
            ),
        ),
        "_cc_toolchain": attr.label(
            default = Label("@bazel_tools//tools/cpp:current_cc_toolchain"),
        ),
        "_experimental_use_coverage_metadata_files": attr.label(
            default = Label("//rust/settings:experimental_use_coverage_metadata_files"),
        ),
        "_experimental_use_global_allocator": attr.label(
            default = Label("//rust/settings:experimental_use_global_allocator"),
            doc = (
                "Label to a boolean build setting that informs the target build whether a global allocator is being used." +
                "This flag is only relevant when used together with --@rules_rust//rust/settings:experimental_use_global_allocator."
            ),
        ),
        "_incompatible_change_rust_test_compilation_output_directory": attr.label(
            default = Label("//rust/settings:incompatible_change_rust_test_compilation_output_directory"),
        ),
        "_incompatible_do_not_include_data_in_compile_data": attr.label(
            default = Label("//rust/settings:incompatible_do_not_include_data_in_compile_data"),
            doc = "Label to a boolean build setting that controls whether to include data files in compile_data.",
        ),
        "_lto": attr.label(
            providers = [RustLtoInfo],
            default = Label("//rust/settings:lto"),
        ),
        "_no_std": attr.label(
            default = Label("//rust/settings:no_std"),
        ),
        "_pipelined_compilation": attr.label(
            default = Label("//rust/settings:pipelined_compilation"),
        ),
        "_rename_first_party_crates": attr.label(
            default = Label("//rust/settings:rename_first_party_crates"),
        ),
        "_third_party_dir": attr.label(
            default = Label("//rust/settings:third_party_dir"),
        ),
        "_toolchain_generated_sysroot": attr.label(
            default = Label("//rust/settings:toolchain_generated_sysroot"),
            doc = (
                "Label to a boolean build setting that lets the rule knows wheter to set --sysroot to rustc. " +
                "This flag is only relevant when used together with --@rules_rust//rust/settings:toolchain_generated_sysroot."
            ),
        ),
    },
    toolchains = [
        "@bazel_tools//tools/cpp:toolchain_type",
    ],
    doc = """Declares a Rust toolchain for use.

This is for declaring a custom toolchain, eg. for configuring a particular version of rust or supporting a new platform.

Example:

Suppose the core rust team has ported the compiler to a new target CPU, called `cpuX`. This \
support can be used in Bazel by defining a new toolchain definition and declaration:

```python
load('@rules_rust//rust:toolchain.bzl', 'rust_toolchain')

rust_toolchain(
    name = "rust_cpuX_impl",
    binary_ext = "",
    dylib_ext = ".so",
    exec_triple = "cpuX-unknown-linux-gnu",
    rust_doc = "@rust_cpuX//:rustdoc",
    rust_std = "@rust_cpuX//:rust_std",
    rustc = "@rust_cpuX//:rustc",
    rustc_lib = "@rust_cpuX//:rustc_lib",
    staticlib_ext = ".a",
    stdlib_linkflags = ["-lpthread", "-ldl"],
    target_triple = "cpuX-unknown-linux-gnu",
)

toolchain(
    name = "rust_cpuX",
    exec_compatible_with = [
        "@platforms//cpu:cpuX",
        "@platforms//os:linux",
    ],
    target_compatible_with = [
        "@platforms//cpu:cpuX",
        "@platforms//os:linux",
    ],
    toolchain = ":rust_cpuX_impl",
)
```

Then, either add the label of the toolchain rule to `register_toolchains` in the WORKSPACE, or pass \
it to the `"--extra_toolchains"` flag for Bazel, and it will be used.

See `@rules_rust//rust:repositories.bzl` for examples of defining the `@rust_cpuX` repository \
with the actual binaries and libraries.
""",
)
