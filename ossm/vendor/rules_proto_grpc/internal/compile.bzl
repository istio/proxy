"""Compilation rules definition for rules_proto_grpc."""

load("@rules_proto//proto:defs.bzl", "ProtoInfo")
load(
    "//internal:common.bzl",
    "copy_file",
    "descriptor_proto_path",
    "get_output_filename",
    "get_package_root",
    "strip_path_prefix",
)
load("//internal:providers.bzl", "ProtoCompileInfo", "ProtoPluginInfo")
load("//internal:protoc.bzl", "build_protoc_args")

proto_compile_attrs = {
    "protos": attr.label_list(
        mandatory = True,
        providers = [ProtoInfo],
        doc = "List of labels that provide the ProtoInfo provider (such as proto_library from rules_proto)",
    ),
    "options": attr.string_list_dict(
        doc = "Extra options to pass to plugins, as a dict of plugin label -> list of strings. The key * can be used exclusively to apply to all plugins",
    ),
    "verbose": attr.int(
        doc = "The verbosity level. Supported values and results are 0: Show nothing, 1: Show command, 2: Show command and sandbox after running protoc, 3: Show command and sandbox before and after running protoc, 4. Show env, command, expected outputs and sandbox before and after running protoc",
    ),
    "prefix_path": attr.string(
        doc = "Path to prefix to the generated files in the output directory",
    ),
    "extra_protoc_args": attr.string_list(
        doc = "A list of extra args to pass directly to protoc, not as plugin options",
    ),
    "extra_protoc_files": attr.label_list(
        allow_files = True,
        doc = "List of labels that provide extra files to be available during protoc execution",
    ),
    "output_mode": attr.string(
        default = "PREFIXED",
        values = ["PREFIXED", "NO_PREFIX", "NO_PREFIX_FLAT"],
        doc = "The output mode for the target. PREFIXED (the default) will output to a directory named by the target within the current package root, NO_PREFIX will output files directly to the current package, NO_PREFIX_FLAT will ouput directly to the current package without mirroring the package tree. Using NO_PREFIX may lead to conflicting writes",
    ),
}

def proto_compile_impl(ctx):
    """
    Common implementation function for lang_*_compile rules.

    Args:
        ctx: The Bazel rule execution context object.

    Returns:
        Providers:
            - ProtoCompileInfo
            - DefaultInfo

    """

    # Load attrs that we pass as args
    # This is done to allow writing rules that can call proto_compile with mutable attributes,
    # such as in doc_template_compile
    options = ctx.attr.options
    extra_protoc_args = getattr(ctx.attr, "extra_protoc_args", [])
    extra_protoc_files = ctx.files.extra_protoc_files

    # Execute with extracted attrs
    return proto_compile(ctx, options, extra_protoc_args, extra_protoc_files)

def proto_compile(ctx, options, extra_protoc_args, extra_protoc_files):
    """
    Common implementation function for lang_*_compile rules.

    Args:
        ctx: The Bazel rule execution context object.
        options: The mutable options dict.
        extra_protoc_args: The mutable extra_protoc_args list.
        extra_protoc_files: The mutable extra_protoc_files list.

    Returns:
        Providers:
            - ProtoCompileInfo
            - DefaultInfo

    """

    # Load attrs
    proto_infos = [dep[ProtoInfo] for dep in ctx.attr.protos]
    plugins = [plugin[ProtoPluginInfo] for plugin in ctx.attr._plugins]
    verbose = ctx.attr.verbose

    # Load toolchain and tools
    protoc_toolchain_info = ctx.toolchains[str(Label("//protobuf:toolchain_type"))]
    protoc = protoc_toolchain_info.protoc_executable
    fixer = protoc_toolchain_info.fixer_executable

    # The directory where the outputs will be generated, relative to the package.
    # A temporary dir is used here to allow output directories that may need to be merged later
    rel_premerge_root = "_rpg_premerge_" + ctx.label.name

    # The full path to the pre-merge output root, relative to the workspace
    premerge_root = get_package_root(ctx) + "/" + rel_premerge_root

    # The lists of generated files and directories that we expect to be produced, in their pre-merge
    # locations
    premerge_files = []
    premerge_dirs = []

    # Convert options dict to label keys
    plugin_labels = [plugin.label for plugin in plugins]
    per_plugin_options = {
        # Dict of plugin label to options string list
        Label(plugin_label): opts
        for plugin_label, opts in options.items()
        if plugin_label != "*"
    }

    # Only allow '*' by itself
    all_plugin_options = []  # Options applied to all plugins, from the '*' key
    if "*" in options:
        if len(options) > 1:
            fail("The options attr on target {} cannot contain '*' and other labels. Use either '*' or labels".format(ctx.label))
        all_plugin_options = options["*"]

    # Check all labels match a plugin in use
    for plugin_label in per_plugin_options:
        if plugin_label not in plugin_labels:
            fail("The options attr on target {} contains a plugin label {} for a plugin that does not exist on this rule. The available plugins are {} ".format(ctx.label, plugin_label, plugin_labels))

    ###
    ### Setup plugins
    ###

    # Each plugin is isolated to its own execution of protoc, as plugins may have differing
    # exclusions that cannot be expressed in a single protoc execution for all plugins.

    for plugin in plugins:
        ###
        ### Check plugin
        ###

        # Check plugin outputs
        if plugin.output_directory and (plugin.out or plugin.outputs or plugin.empty_template):
            fail("Proto plugin {} cannot use output_directory in conjunction with outputs, out or empty_template".format(plugin.name))

        ###
        ### Gather proto files and filter by exclusions
        ###

        protos = []  # The filtered set of .proto files to compile
        plugin_outputs = []
        proto_paths = []  # The paths passed to protoc
        for proto_info in proto_infos:
            for proto in proto_info.direct_sources:
                # Check for exclusion
                if any([
                    proto.dirname.endswith(exclusion) or proto.path.endswith(exclusion)
                    for exclusion in plugin.exclusions
                ]) or proto in protos:
                    # When using import_prefix, the ProtoInfo.direct_sources list appears to contain
                    # duplicate records, the final check 'proto in protos' removes these. See
                    # https://github.com/bazelbuild/bazel/issues/9127
                    continue

                # Proto not excluded
                protos.append(proto)

                # Add per-proto outputs
                for pattern in plugin.outputs:
                    plugin_outputs.append(ctx.actions.declare_file("{}/{}".format(
                        rel_premerge_root,
                        get_output_filename(proto, pattern, proto_info),
                    )))

                # Get proto path for protoc
                proto_paths.append(descriptor_proto_path(proto, proto_info))

        # Skip plugin if all proto files have now been excluded
        if len(protos) == 0:
            if verbose > 2:
                print(
                    'Skipping plugin "{}" for "{}" as all proto files have been excluded'.format(
                        plugin.name,
                        ctx.label,
                    ),
                )  # buildifier: disable=print
            continue

        # Append current plugin outputs to global outputs before looking at per-plugin outputs;
        # these are manually added globally as there may be srcjar outputs.
        premerge_files.extend(plugin_outputs)

        ###
        ### Declare per-plugin outputs
        ###

        # Some protoc plugins generate a set of output files (like python) while others generate a
        # single 'archive' file that contains the individual outputs (like java). Jar outputs are
        # gathered as a special case as we need to post-process them to have a 'srcjar' extension
        # (java_library rules don't accept source jars with a 'jar' extension).

        out_file = None
        if plugin.out:
            # Define out file
            out_file = ctx.actions.declare_file("{}/{}".format(
                rel_premerge_root,
                plugin.out.replace("{name}", ctx.label.name),
            ))
            plugin_outputs.append(out_file)

            if not out_file.path.endswith(".jar"):
                # Add output direct to global outputs
                premerge_files.append(out_file)
            else:
                # Create .srcjar from .jar for global outputs
                premerge_files.append(copy_file(
                    ctx,
                    out_file,
                    "{}.srcjar".format(out_file.basename.rpartition(".")[0]),
                    sibling = out_file,
                ))

        ###
        ### Declare plugin output directory if required
        ###

        # Some plugins outputs a structure that cannot be predicted from the input file paths alone.
        # For these plugins, we simply declare the directory.

        if plugin.output_directory:
            out_file = ctx.actions.declare_directory(rel_premerge_root + "/" + "_plugin_" + plugin.name)
            plugin_outputs.append(out_file)
            premerge_dirs.append(out_file)

        ###
        ### Build command
        ###

        # Determine the outputs expected by protoc.
        # When plugin.empty_template is not set, protoc will output directly to the final targets.
        # When set, we will direct the plugin outputs to a temporary folder, then use the fixer
        # executable to write to the final targets.
        if plugin.empty_template:
            # Create path list for fixer
            fixer_paths_file = ctx.actions.declare_file(rel_premerge_root + "/" + "_plugin_fixer_manifest_" + plugin.name + ".txt")
            ctx.actions.write(fixer_paths_file, "\n".join([
                file.path.partition(premerge_root + "/")[2]  # Path of the file relative to the output root
                for file in plugin_outputs
            ]))

            # Create output directory for protoc to write into
            fixer_dir = ctx.actions.declare_directory(
                rel_premerge_root + "/" + "_plugin_fixed_" + plugin.name,
            )
            out_arg = fixer_dir.path
            plugin_protoc_outputs = [fixer_dir]

            # Apply fixer
            ctx.actions.run(
                inputs = [fixer_paths_file, fixer_dir, plugin.empty_template],
                outputs = plugin_outputs,
                arguments = [
                    fixer_paths_file.path,
                    plugin.empty_template.path,
                    fixer_dir.path,
                    premerge_root,
                ],
                progress_message = "Applying fixer for {} plugin on target {}".format(
                    plugin.name,
                    ctx.label,
                ),
                executable = fixer,
            )

        else:
            # No fixer, protoc writes files directly
            if out_file and "QUIRK_OUT_PASS_ROOT" not in plugin.quirks:
                # Single output file, pass the full file name to out arg, unless QUIRK_OUT_PASS_ROOT
                # quirk is in use
                out_arg = out_file.path
            else:
                # No single output (or QUIRK_OUT_PASS_ROOT enabled), pass root dir
                out_arg = premerge_root
            plugin_protoc_outputs = plugin_outputs

        # Build argument list for protoc execution
        args_list, cmd_inputs, cmd_input_manifests = build_protoc_args(
            ctx,
            plugin,
            proto_infos,
            out_arg,
            extra_options = all_plugin_options + per_plugin_options.get(plugin.label, []),
            extra_protoc_args = extra_protoc_args,
        )
        args = ctx.actions.args()
        args.set_param_file_format("multiline")
        args.use_param_file("@%s", use_always = False)
        args.add_all(args_list)

        # Add import roots and files if required by plugin
        # By default we pass just the descriptors and the proto paths, but these may not contain
        # all of the comments etc from the source files
        if "QUIRK_DIRECT_MODE" in plugin.quirks:
            args.add_all([
                "--proto_path=" + proto_info.proto_source_root
                for proto_info in proto_infos
            ])
            cmd_inputs += protos

        # Add source proto files as descriptor paths
        for proto_path in proto_paths:
            args.add(proto_path)

        ###
        ### Specify protoc action
        ###

        # $@ is replaced with args list and is quote wrapped to support paths with special chars
        mnemonic = "ProtoCompile"
        command = ("mkdir -p '{}' && ".format(premerge_root)) + protoc.path + ' "$@"'
        cmd_inputs += extra_protoc_files
        tools = [protoc] + ([plugin.tool_executable] if plugin.tool_executable else [])

        # Amend command with debug options
        if verbose > 0:
            print("{}:".format(mnemonic), protoc.path, args)  # buildifier: disable=print

        if verbose > 1:
            command += " && echo '\n##### SANDBOX AFTER RUNNING PROTOC' && find . -type f "

        if verbose > 2:
            command = "echo '\n##### SANDBOX BEFORE RUNNING PROTOC' && find . -type l && " + command

        if verbose > 3:
            command = "env && " + command
            for f in cmd_inputs:
                print("INPUT:", f.path)  # buildifier: disable=print
            for f in protos:
                print("TARGET PROTO:", f.path)  # buildifier: disable=print
            for f in tools:
                print("TOOL:", f.path)  # buildifier: disable=print
            for f in plugin_outputs:
                print("EXPECTED OUTPUT:", f.path)  # buildifier: disable=print

        # Check env attr exclusivity
        if plugin.env and plugin.use_built_in_shell_environment:
            fail(
                "Plugin env and use_built_in_shell_environment attributes are mutually exclusive; " +
                " both set for plugin {}".format(plugin.name),
            )

        # Build protoc env for plugin, with replacement
        # See https://github.com/rules-proto-grpc/rules_proto_grpc/pull/226
        plugin_env = {
            k: v.replace("{bindir}", ctx.bin_dir.path)
            for k, v in plugin.env.items()
        }

        # Run protoc (https://bazel.build/rules/lib/actions#run_shell)
        ctx.actions.run_shell(
            mnemonic = mnemonic,
            command = command,
            arguments = [args],
            inputs = cmd_inputs,
            tools = tools,
            outputs = plugin_protoc_outputs,
            env = plugin_env,
            use_default_shell_env = plugin.use_built_in_shell_environment,
            input_manifests = cmd_input_manifests,
            progress_message = "Compiling protoc outputs for {} plugin on target {}".format(
                plugin.name,
                ctx.label,
            ),
        )

    # Build final output defaults for merged locations
    output_root = get_package_root(ctx) + "/" + ctx.label.name
    output_files = depset()
    output_dirs = depset()
    prefix_path = ctx.attr.prefix_path

    # Merge outputs
    if premerge_dirs:
        # If we have any output dirs specified, we declare a single output directory and merge all
        # files in one go. This is necessary to prevent path prefix conflicts
        if ctx.attr.output_mode != "PREFIXED":
            fail("Cannot use output_mode = {} when using plugins with directory outputs")

        # Declare single output directory
        dir_name = ctx.label.name
        if prefix_path:
            dir_name += "/" + prefix_path
        new_dir = ctx.actions.declare_directory(dir_name)
        output_dirs = depset(direct = [new_dir])

        # Build copy command for directory outputs
        # Use cp {}/. rather than {}/* to allow for empty output directories from a plugin (e.g when
        # no service exists, so no files generated)
        command_parts = ["mkdir -p {} && cp -r {} '{}'".format(
            # We need to be sure that the dirs exist, see:
            # https://github.com/bazelbuild/bazel/issues/6393
            " ".join(["'" + d.path + "'" for d in premerge_dirs]),
            " ".join(["'" + d.path + "/.'" for d in premerge_dirs]),
            new_dir.path,
        )]

        # Extend copy command with file outputs
        command_input_files = premerge_dirs
        for file in premerge_files:
            # Strip pre-merge root from file path
            path = strip_path_prefix(file.path, premerge_root)

            # Prefix path is contained in new_dir.path created above and
            # used below

            # Add command to copy file to output
            command_input_files.append(file)
            command_parts.append("mkdir -p $(dirname '{}')".format(
                "{}/{}".format(new_dir.path, path),
            ))
            command_parts.append("cp '{}' '{}'".format(
                file.path,
                "{}/{}".format(new_dir.path, path),
            ))

        # Add debug options
        if verbose > 1:
            command_parts = command_parts + [
                "echo '\n##### SANDBOX AFTER MERGING DIRECTORIES'",
                "find . -type l",
            ]
        if verbose > 2:
            command_parts = [
                "echo '\n##### SANDBOX BEFORE MERGING DIRECTORIES'",
                "find . -type l",
            ] + command_parts
        if verbose > 0:
            print(
                "Directory merge command: {}".format(" && ".join(command_parts)),
            )  # buildifier: disable=print

        # Copy directories and files to shared output directory in one action
        ctx.actions.run_shell(
            mnemonic = "CopyDirs",
            inputs = command_input_files,
            outputs = [new_dir],
            command = " && ".join(command_parts),
            progress_message = "copying directories and files to {}".format(new_dir.path),
        )

    else:
        # Otherwise, if we only have output files, build the output tree by
        # aggregating files into one directory

        output_files = []
        for file in premerge_files:
            # Strip pre-merge root from file path
            path = strip_path_prefix(file.path, premerge_root)

            # Prepend prefix path if given
            if prefix_path:
                path = prefix_path + "/" + path

            # Select output location based on output mode
            # In PREFIXED mode we output to a directory named by the target label
            # In NO_PREFIX or NO_PREFIX_FLAT mode, we output directly to the package root
            if ctx.attr.output_mode == "PREFIXED":
                path = ctx.label.name + "/" + path
            elif ctx.attr.output_mode == "NO_PREFIX_FLAT":
                path = file.basename

            # Copy file to output
            output_files.append(copy_file(
                ctx,
                file,
                path,
            ))

        output_files = depset(direct = output_files)

    # Create depset containing all outputs
    all_outputs = depset(direct = output_files.to_list() + output_dirs.to_list())

    # Create default and proto compile providers
    return [
        ProtoCompileInfo(
            label = ctx.label,
            output_root = output_root,
            output_files = output_files,
            output_dirs = output_dirs,
        ),
        DefaultInfo(
            files = all_outputs,
            runfiles = ctx.runfiles(transitive_files = all_outputs),
        ),
    ]
