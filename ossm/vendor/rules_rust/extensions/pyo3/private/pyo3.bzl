"""Bazel pyo3 rules"""

load("@bazel_skylib//lib:paths.bzl", "paths")
load("@rules_python//python:defs.bzl", "PyInfo")
load(
    "@rules_rust//rust:defs.bzl",
    "rust_analyzer_aspect",
    "rust_clippy_aspect",
    "rust_common",
    "rust_shared_library",
    "rustfmt_aspect",
)
load(":pyo3_toolchain.bzl", "PYO3_TOOLCHAIN")

def _compilation_mode_transition_impl(settings, attr):
    output = dict(settings)
    if attr.compilation_mode in ["dbg", "fastbuild", "opt"]:
        output["//command_line_option:compilation_mode"] = attr.compilation_mode
    return output

_compilation_mode_transition = transition(
    implementation = _compilation_mode_transition_impl,
    inputs = ["//command_line_option:compilation_mode"],
    outputs = ["//command_line_option:compilation_mode"],
)

def _get_imports(ctx, imports):
    """Determine the import paths from a target's `imports` attribute.

    Args:
        ctx (ctx): The rule's context object.
        imports (list): A list of import paths.

    Returns:
        depset: A set of the resolved import paths.
    """
    workspace_name = ctx.label.workspace_name
    if not workspace_name:
        workspace_name = ctx.workspace_name

    import_root = "{}/{}".format(workspace_name, ctx.label.package).rstrip("/")

    result = [workspace_name]
    for import_str in imports:
        import_str = ctx.expand_make_variables("imports", import_str, {})
        if import_str.startswith("/"):
            continue

        # To prevent "escaping" out of the runfiles tree, we normalize
        # the path and ensure it doesn't have up-level references.
        import_path = paths.normalize("{}/{}".format(import_root, import_str))
        if import_path.startswith("../") or import_path == "..":
            fail("Path '{}' references a path above the execution root".format(
                import_str,
            ))
        result.append(import_path)

    return depset(result)

def _stubs_enabled(stubs_flag, toolchain):
    """Determine if stubs should be generated.

    Args:
        stubs_flag (int): The `py_pyo3_library.stubs` attribute.
        toolchain (pyo3_toolchain): The current toolchain.

    Returns:
        bool: Whether or not to generate stubs.
    """

    if stubs_flag == 1:
        return True

    if stubs_flag == 0:
        return False

    return toolchain.experimental_stubgen

def _py_pyo3_library_impl(ctx):
    toolchain = ctx.toolchains[PYO3_TOOLCHAIN]

    files = []

    crate_info = ctx.attr.extension[rust_common.test_crate_info].crate

    extension = crate_info.output
    is_windows = extension.basename.endswith(".dll")

    # https://pyo3.rs/v0.26.0/building-and-distribution#manual-builds
    #
    # Determine the on-disk and logical Python module layout.
    #
    # `module` is a full dotted module path (e.g. "foo.bar"). We split on the
    # last "." such that:
    #   - module_prefix == "foo"
    #   - module_name == "bar"
    #
    # `module_name` must match the `#[pymodule] fn <name>(...)` in the Rust code
    # and is also what we pass to the stub generator.
    module_path = ctx.attr.module_name if ctx.attr.module_name else ctx.label.name.replace("/", ".")

    if module_path.startswith(".") or module_path.endswith(".") or ".." in module_path:
        fail("Invalid `module` value '{}': expected a dotted module path like 'foo.bar'.".format(module_path))

    last_dot = module_path.rfind(".")
    if last_dot == -1:
        module_prefix = None
        module_name = module_path
    else:
        module_prefix = module_path[:last_dot]
        module_name = module_path[last_dot + 1:]

    if not module_name:
        fail("Invalid `module` value '{}': module name may not be empty.".format(module_path))

    # Convert module_prefix (e.g. "foo.bar") into a path ("foo/bar") and place
    # the extension and stubs in the corresponding directory.
    if module_prefix:
        module_prefix_path = module_prefix.replace(".", "/")
        module_relpath = "{}/{}.{}".format(module_prefix_path, module_name, "pyd" if is_windows else "so")
        stub_relpath = "{}/{}.pyi".format(module_prefix_path, module_name)
    else:
        module_relpath = "{}.{}".format(module_name, "pyd" if is_windows else "so")
        stub_relpath = "{}.pyi".format(module_name)

    ext = ctx.actions.declare_file(module_relpath)
    ctx.actions.symlink(
        output = ext,
        target_file = extension,
    )
    files.append(ext)

    stub = None
    if _stubs_enabled(ctx.attr.stubs, toolchain):
        stub = ctx.actions.declare_file(stub_relpath)

        args = ctx.actions.args()
        args.add(module_name, format = "--module_name=%s")
        args.add(ext, format = "--module_path=%s")
        args.add(stub, format = "--output=%s")
        ctx.actions.run(
            mnemonic = "PyO3StubGen",
            outputs = [stub],
            inputs = [ext],
            executable = ctx.executable._stubgen,
            arguments = [args],
        )
        files.append(stub)

    providers = [
        DefaultInfo(
            files = depset([ext]),
            runfiles = ctx.runfiles(transitive_files = depset(files, transitive = [crate_info.data])),
        ),
        PyInfo(
            imports = _get_imports(ctx, ctx.attr.imports),
            transitive_sources = depset(),
        ),
        coverage_common.instrumented_files_info(
            ctx,
            dependency_attributes = ["extension"],
        ),
    ]

    # Forward any aspect-generated outputs for known rules_rust aspects.
    if OutputGroupInfo in ctx.attr.extension:
        output_info = ctx.attr.extension[OutputGroupInfo]
        output_groups = {}
        for group in ["rusfmt_checks", "clippy_checks", "rust_analyzer_crate_spec"]:
            if hasattr(output_info, group):
                output_groups[group] = getattr(output_info, group)

        if stub:
            output_groups["pyo3_type_stubs"] = depset([stub])

        providers.append(OutputGroupInfo(**output_groups))

    return providers

py_pyo3_library = rule(
    doc = "Define a Python library for a PyO3 extension.",
    implementation = _py_pyo3_library_impl,
    cfg = _compilation_mode_transition,
    attrs = {
        "compilation_mode": attr.string(
            doc = (
                "Specify the mode `extension` will be built in. For details see " +
                " [`--compilation_mode`](https://bazel.build/reference/command-line-reference#flag--compilation_mode)"
            ),
            values = [
                "dbg",
                "fastbuild",
                "opt",
                "current",
            ],
            default = "opt",
        ),
        "extension": attr.label(
            doc = "The PyO3 library.",
            cfg = "target",
            # `rust_shared_library` does not provide `CrateInfo` but
            # does contain `TestCrateInfo` which wraps the data we need.
            providers = [rust_common.test_crate_info],
            # Ensure common linters are run on the extension and yielded by
            # this rule for ease of access.
            aspects = [
                rust_analyzer_aspect,
                rust_clippy_aspect,
                rustfmt_aspect,
            ],
            mandatory = True,
        ),
        "imports": attr.string_list(
            doc = "List of import directories to be added to the `PYTHONPATH`.",
        ),
        "module_name": attr.string(
            doc = "A full dotted Python module path implemented by this extension (e.g. `foo.bar`).",
        ),
        "stubs": attr.int(
            doc = "Whether or not to generate stubs. `-1` will default to the global config, `0` will never generate, and `1` will always generate stubs.",
            default = -1,
            values = [
                -1,
                0,
                1,
            ],
        ),
        "_stubgen": attr.label(
            doc = "A binary used to generate pythons type stubs.",
            cfg = "exec",
            executable = True,
            default = Label("//private:stubgen"),
        ),
    },
    toolchains = [PYO3_TOOLCHAIN],
)

def pyo3_extension(
        *,
        name,
        srcs,
        aliases = {},
        compile_data = [],
        crate_features = [],
        crate_root = None,
        data = [],
        deps = [],
        edition = None,
        imports = [],
        proc_macro_deps = [],
        rustc_env = {},
        rustc_env_files = [],
        rustc_flags = [],
        stubs = None,
        version = None,
        compilation_mode = "opt",
        module_name = None,
        **kwargs):
    """Define a PyO3 python extension module.

    This target is consumed just as a `py_library` would be.

    [rsl]: https://bazelbuild.github.io/rules_rust/defs.html#rust_shared_library
    [pli]: https://bazel.build/reference/be/python#py_binary.imports

    Args:
        name (str): The name of the target.
        srcs (list): List of Rust `.rs` source files used to build the library.
            For more details see [rust_shared_library][rsl].
        aliases (dict, optional): Remap crates to a new name or moniker for linkage to this target.
            For more details see [rust_shared_library][rsl].
        compile_data (list, optional): List of files used by this rule at compile time.
            For more details see [rust_shared_library][rsl].
        crate_features (list, optional): List of features to enable for this crate.
            For more details see [rust_shared_library][rsl].
        crate_root (Label, optional): The file that will be passed to `rustc` to be used for building this crate.
            For more details see [rust_shared_library][rsl].
        data (list, optional): List of files used by this rule at compile time and runtime.
            For more details see [rust_shared_library][rsl].
        deps (list, optional): List of other libraries to be linked to this library target.
            For more details see [rust_shared_library][rsl].
        edition (str, optional): The rust edition to use for this crate. Defaults to the edition specified in the rust_toolchain.
            For more details see [rust_shared_library][rsl].
        imports (list, optional): List of import directories to be added to the `PYTHONPATH`.
            For more details see [py_library.imports][pli].
        proc_macro_deps (list, optional): List of `rust_proc_macro` targets used to help build this library target.
            For more details see [rust_shared_library][rsl].
        rustc_env (dict, optional): Dictionary of additional `"key": "value"` environment variables to set for rustc.
            For more details see [rust_shared_library][rsl].
        rustc_env_files (list, optional): Files containing additional environment variables to set for rustc.
            For more details see [rust_shared_library][rsl].
        rustc_flags (list, optional): List of compiler flags passed to `rustc`.
            For more details see [rust_shared_library][rsl].
        stubs (bool, optional): Whether or not to generate stubs (`.pyi` file) for the module.
        version (str, optional): A version to inject in the cargo environment variable.
            For more details see [rust_shared_library][rsl].
        compilation_mode (str, optional): The [compilation_mode](https://bazel.build/reference/command-line-reference#flag--compilation_mode)
            value to build the extension for. If set to `"current"`, the current configuration will be used.
        module_name (str, optional): A full dotted Python module path implemented by this extension (e.g. `foo.bar`).
        **kwargs (dict): Additional keyword arguments.
    """
    tags = kwargs.pop("tags", [])
    visibility = kwargs.pop("visibility", None)

    # Add macOS-specific flags

    macos_flags = select({
        "@platforms//os:macos": [
            # https://pyo3.rs/v0.26.0/building-and-distribution.html#macos
            "-C",
            "link-arg=-undefined",
            "-C",
            "link-arg=dynamic_lookup",
            # Required due to: https://github.com/PyO3/pyo3/issues/5035
            "--codegen=link-arg=-Wl,-no_fixup_chains",
        ],
        "//conditions:default": [],
    })

    all_rustc_flags = rustc_flags + macos_flags

    rust_shared_library(
        name = name + "_shared",
        srcs = srcs,
        aliases = aliases,
        compile_data = compile_data,
        crate_features = crate_features,
        crate_name = kwargs.pop("crate_name", name),
        crate_root = crate_root,
        data = data,
        deps = [
            Label("//private:current_rust_pyo3_toolchain"),
            Label("@rules_python//python/cc:current_py_cc_headers"),
        ] + deps,
        edition = edition,
        proc_macro_deps = proc_macro_deps,
        rustc_env = rustc_env,
        rustc_env_files = rustc_env_files,
        rustc_flags = all_rustc_flags,
        tags = depset(tags + ["manual"]).to_list(),
        version = version,
        visibility = ["//visibility:private"],
        **kwargs
    )

    if stubs == None:
        stubs_int = -1
    elif stubs:
        stubs_int = 1
    else:
        stubs_int = 0

    py_pyo3_library(
        name = name,
        extension = name + "_shared",
        compilation_mode = compilation_mode,
        stubs = stubs_int,
        imports = imports,
        module_name = module_name,
        tags = tags,
        visibility = visibility,
        **kwargs
    )
