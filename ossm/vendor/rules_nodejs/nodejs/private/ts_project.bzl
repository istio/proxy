"ts_project rule"

load("@bazel_skylib//lib:dicts.bzl", "dicts")
load("@rules_nodejs//nodejs/private/providers:declaration_info.bzl", "DeclarationInfo", "declaration_info")
load("@rules_nodejs//nodejs/private/providers:js_providers.bzl", "js_module_info")
load(":ts_lib.bzl", "COMPILER_OPTION_ATTRS", "OUTPUT_ATTRS", "STD_ATTRS", "ValidOptionsInfo", _lib = "lib")
load(":ts_config.bzl", "TsConfigInfo")
load(":ts_validate_options.bzl", _validate_lib = "lib")

def _ts_project_impl(ctx, run_action = None, ExternalNpmPackageInfo = None):
    """Creates the action which spawns `tsc`.

    This function has two extra arguments that are particular to how it's called
    within build_bazel_rules_nodejs and @bazel/typescript npm package.
    Other TS rule implementations wouldn't need to pass these:

    Args:
        ctx: starlark rule execution context
        run_action: used with the build_bazel_rules_nodejs linker, by default we use ctx.actions.run
        ExternalNpmPackageInfo: a provider symbol specific to the build_bazel_rules_nodejs linker

    Returns:
        list of providers
    """
    srcs = [_lib.relative_to_package(src.path, ctx) for src in ctx.files.srcs]

    # Recalculate outputs inside the rule implementation.
    # The outs are first calculated in the macro in order to try to predetermine outputs so they can be declared as
    # outputs on the rule. This provides the benefit of being able to reference an output file with a label.
    # However, it is not possible to evaluate files in outputs of other rules such as filegroup, therefore the outs are
    # recalculated here.
    typings_out_dir = ctx.attr.declaration_dir or ctx.attr.out_dir
    js_outs = _lib.declare_outputs(ctx, [] if not ctx.attr.transpile else _lib.calculate_js_outs(srcs, ctx.attr.out_dir, ctx.attr.root_dir, ctx.attr.allow_js, ctx.attr.preserve_jsx, ctx.attr.emit_declaration_only))
    map_outs = _lib.declare_outputs(ctx, [] if not ctx.attr.transpile else _lib.calculate_map_outs(srcs, ctx.attr.out_dir, ctx.attr.root_dir, ctx.attr.source_map, ctx.attr.preserve_jsx, ctx.attr.emit_declaration_only))
    typings_outs = _lib.declare_outputs(ctx, _lib.calculate_typings_outs(srcs, typings_out_dir, ctx.attr.root_dir, ctx.attr.declaration, ctx.attr.composite, ctx.attr.allow_js))
    typing_maps_outs = _lib.declare_outputs(ctx, _lib.calculate_typing_maps_outs(srcs, typings_out_dir, ctx.attr.root_dir, ctx.attr.declaration_map, ctx.attr.allow_js))

    arguments = ctx.actions.args()
    execution_requirements = {}
    progress_prefix = "Compiling TypeScript project"

    if ctx.attr.supports_workers:
        # Set to use a multiline param-file for worker mode
        arguments.use_param_file("@%s", use_always = True)
        arguments.set_param_file_format("multiline")
        execution_requirements["supports-workers"] = "1"
        execution_requirements["worker-key-mnemonic"] = "TsProject"
        progress_prefix = "Compiling TypeScript project (worker mode)"

    # Add user specified arguments *before* rule supplied arguments
    arguments.add_all(ctx.attr.args)

    arguments.add_all([
        "--project",
        ctx.file.tsconfig.path,
        "--outDir",
        _lib.join(ctx.bin_dir.path, ctx.label.workspace_root, ctx.label.package, ctx.attr.out_dir),
        "--rootDir",
        _lib.calculate_root_dir(ctx),
    ])
    if len(typings_outs) > 0:
        declaration_dir = ctx.attr.declaration_dir if ctx.attr.declaration_dir else ctx.attr.out_dir
        arguments.add_all([
            "--declarationDir",
            _lib.join(ctx.bin_dir.path, ctx.label.workspace_root, ctx.label.package, declaration_dir),
        ])

    # When users report problems, we can ask them to re-build with
    # --define=VERBOSE_LOGS=1
    # so anything that's useful to diagnose rule failures belongs here
    if "VERBOSE_LOGS" in ctx.var.keys():
        arguments.add_all([
            # What files were in the ts.Program
            "--listFiles",
            # Did tsc write all outputs to the place we expect to find them?
            "--listEmittedFiles",
            # Why did module resolution fail?
            "--traceResolution",
            # Why was the build slow?
            "--diagnostics",
            "--extendedDiagnostics",
        ])

    transitive_inputs = []
    inputs = ctx.files.srcs[:]
    for dep in ctx.attr.deps:
        if TsConfigInfo in dep:
            transitive_inputs.append(dep[TsConfigInfo].deps)
        if ExternalNpmPackageInfo != None and ExternalNpmPackageInfo in dep:
            # TODO: we could maybe filter these to be tsconfig.json or *.d.ts only
            # we don't expect tsc wants to read any other files from npm packages.
            transitive_inputs.append(dep[ExternalNpmPackageInfo].sources)
        if DeclarationInfo in dep:
            transitive_inputs.append(dep[DeclarationInfo].transitive_declarations)
        if ValidOptionsInfo in dep:
            transitive_inputs.append(depset([dep[ValidOptionsInfo].marker]))

    # Gather TsConfig info from both the direct (tsconfig) and indirect (extends) attribute
    tsconfig_inputs = _validate_lib.tsconfig_inputs(ctx)
    transitive_inputs.append(tsconfig_inputs)

    # We do not try to predeclare json_outs, because their output locations generally conflict with their path in the source tree.
    # (The exception is when out_dir is used, then the .json output is a different path than the input.)
    # However tsc will copy .json srcs to the output tree so we want to declare these outputs to include along with .js Default outs
    # NB: We don't have emit_declaration_only setting here, so use presence of any JS outputs as an equivalent.
    # tsc will only produce .json if it also produces .js
    if len(js_outs):
        pkg_len = len(ctx.label.package) + 1 if len(ctx.label.package) else 0
        rootdir_replace_pattern = ctx.attr.root_dir + "/" if ctx.attr.root_dir else ""
        json_outs = _lib.declare_outputs(ctx, [
            _lib.join(ctx.attr.out_dir, src.short_path[pkg_len:].replace(rootdir_replace_pattern, ""))
            for src in ctx.files.srcs
            if src.basename.endswith(".json") and src.is_source
        ])
    else:
        json_outs = []

    outputs = json_outs + js_outs + map_outs + typings_outs + typing_maps_outs
    if ctx.outputs.buildinfo_out:
        arguments.add_all([
            "--tsBuildInfoFile",
            ctx.outputs.buildinfo_out.path,
        ])
        outputs.append(ctx.outputs.buildinfo_out)
    runtime_outputs = json_outs + js_outs + map_outs
    typings_outputs = typings_outs + typing_maps_outs + [s for s in ctx.files.srcs if s.path.endswith(".d.ts")]

    if not js_outs and not typings_outputs and not ctx.attr.deps:
        label = "//{}:{}".format(ctx.label.package, ctx.label.name)
        if ctx.attr.transpile:
            no_outs_msg = """ts_project target %s is configured to produce no outputs.

This might be because
- you configured it with `noEmit`
- the `srcs` are empty
""" % label
        else:
            no_outs_msg = "ts_project target %s with custom transpiler needs `declaration = True`." % label
        fail(no_outs_msg + """
This is an error because Bazel does not run actions unless their outputs are needed for the requested targets to build.
""")

    if ctx.attr.transpile:
        default_outputs_depset = depset(runtime_outputs) if len(runtime_outputs) else depset(typings_outputs)
    else:
        # We must avoid tsc writing any JS files in this case, as tsc was only run for typings, and some other
        # action will try to write the JS files. We must avoid collisions where two actions write the same file.
        arguments.add("--emitDeclarationOnly")

        # We don't produce any DefaultInfo outputs in this case, because we avoid running the tsc action
        # unless the DeclarationInfo is requested.
        default_outputs_depset = depset([])

    if len(outputs) > 0:
        run_action_kwargs = {
            "inputs": depset(inputs, transitive = transitive_inputs),
            "arguments": [arguments],
            "outputs": outputs,
            "mnemonic": "TsProject",
            "execution_requirements": execution_requirements,
            "progress_message": "%s %s [tsc -p %s]" % (
                progress_prefix,
                ctx.label,
                ctx.file.tsconfig.short_path,
            ),
        }
        if run_action != None:
            run_action(
                ctx,
                link_workspace_root = ctx.attr.link_workspace_root,
                executable = "tsc",
                **run_action_kwargs
            )
        else:
            ctx.actions.run(
                executable = ctx.executable.tsc,
                **run_action_kwargs
            )

    providers = [
        # DefaultInfo is what you see on the command-line for a built library,
        # and determines what files are used by a simple non-provider-aware
        # downstream library.
        # Only the JavaScript outputs are intended for use in non-TS-aware
        # dependents.
        DefaultInfo(
            files = default_outputs_depset,
            runfiles = ctx.runfiles(
                transitive_files = depset(ctx.files.data, transitive = [
                    default_outputs_depset,
                ]),
                collect_default = True,
            ),
        ),
        js_module_info(
            sources = depset(runtime_outputs),
            deps = ctx.attr.deps,
        ),
        TsConfigInfo(deps = depset(transitive = [tsconfig_inputs] + [
            dep[TsConfigInfo].deps
            for dep in ctx.attr.deps
            if TsConfigInfo in dep
        ])),
        coverage_common.instrumented_files_info(
            ctx,
            source_attributes = ["srcs"],
            dependency_attributes = ["deps"],
            extensions = ["ts", "tsx"],
        ),
    ]

    # Only provide DeclarationInfo if there are some typings.
    # Improves error messaging if a ts_project is missing declaration = True
    typings_in_deps = [d for d in ctx.attr.deps if DeclarationInfo in d]
    if len(typings_outputs) or len(typings_in_deps):
        providers.append(declaration_info(depset(typings_outputs), typings_in_deps))
        providers.append(OutputGroupInfo(types = depset(typings_outputs)))

    return providers

ts_project = struct(
    implementation = _ts_project_impl,
    attrs = dicts.add(COMPILER_OPTION_ATTRS, STD_ATTRS, OUTPUT_ATTRS),
)
