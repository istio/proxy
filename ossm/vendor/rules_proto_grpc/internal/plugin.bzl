"""Plugin rules definition for rules_proto_grpc."""

load("//internal:providers.bzl", "ProtoPluginInfo")

def _proto_plugin_impl(ctx):
    return [
        ProtoPluginInfo(
            name = ctx.attr.name,
            label = ctx.label,
            tool = ctx.attr.tool,
            tool_executable = ctx.executable.tool,
            protoc_plugin_name = ctx.attr.protoc_plugin_name,
            options = ctx.attr.options,
            outputs = ctx.attr.outputs,
            out = ctx.attr.out,
            output_directory = ctx.attr.output_directory,
            env = ctx.attr.env,
            extra_protoc_args = ctx.attr.extra_protoc_args,
            exclusions = ctx.attr.exclusions,
            data = ctx.files.data,
            use_built_in_shell_environment = ctx.attr.use_built_in_shell_environment,
            separate_options_flag = ctx.attr.separate_options_flag,
            empty_template = ctx.file.empty_template,
            quirks = ctx.attr.quirks,
        ),
    ]

proto_plugin = rule(
    implementation = _proto_plugin_impl,
    attrs = {
        "tool": attr.label(
            doc = "The label of plugin binary target. Can be the output of select() to support multiple platforms. If absent, it is assumed the plugin is built-in to protoc itself and builtin_plugin_name will be used if available, otherwise the plugin name",
            cfg = "exec",
            allow_files = True,
            executable = True,
        ),
        "protoc_plugin_name": attr.string(
            doc = "The name used for the plugin binary on the protoc command line. Useful for targeting built-in plugins. Uses the plugin name when not set",
        ),
        "options": attr.string_list(
            doc = "A list of options to pass to the compiler for this plugin",
        ),
        "outputs": attr.string_list(
            doc = "Templates for output filenames generated on a per-proto basis, such as '{basename}_pb2.py'. The {basename} template variable will be replaced with the file basename and {protopath} with be replaced with the relative path to the .proto file after prefix mangling. If no template variables are present, the string is assumed to be a suffix of the file basename",
        ),
        "out": attr.string(
            doc = "Template for the output filename generated on a per-plugin basis; to be used in the value for --NAME-out=OUT. The {name} template variable will be replaced with the target name",
        ),
        "output_directory": attr.bool(
            doc = "Flag that indicates that the plugin should only output a directory. Used for plugins that have no direct mapping from source file name to output name. Cannot be used in conjunction with outputs or out",
            default = False,
        ),
        "env": attr.string_dict(
            doc = "A dictionary of key-value environment variables to use when invoking protoc for this plugin. Must be empty if use_built_in_shell_environment is true",
            default = {},
        ),
        "extra_protoc_args": attr.string_list(
            doc = "A list of extra command line arguments to pass directly to protoc, not as plugin options",
        ),
        "exclusions": attr.string_list(
            doc = "Exclusion filters to apply when generating outputs with this plugin. Used to prevent generating files that are included in the protobuf library, for example. Can exclude either by proto name prefix or by proto folder prefix",
        ),
        "data": attr.label_list(
            doc = "Additional files required for running the plugin",
            allow_files = True,
        ),
        "use_built_in_shell_environment": attr.bool(
            doc = "Flag to indicate whether the tool should use the built in shell environment",
            default = True,
        ),
        "separate_options_flag": attr.bool(
            doc = "Flag to indicate if plugin options should be sent to protoc via the separate --{lang}_opts argument",
            default = False,
        ),
        "empty_template": attr.label(
            doc = "Template file to use to fill missing outputs when the fixer is required. If not provided, the fixer is not run",
            allow_single_file = True,
        ),
        "quirks": attr.string_list(
            doc = "List of quirks that toggle behaviours in compilation. The QUIRK_OUT_PASS_ROOT quirk enables passing the output directory to a plugin that outputs only a single file. The QUIRK_DIRECT_MODE quirk disables use of descriptors from proto_library and passes the files directly to protoc",
        ),
    },
)
