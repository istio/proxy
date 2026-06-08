"""Test rule for static linking with bazel's builtin Apple logic"""

load("//test:transitions.bzl", "apple_platform_split_transition")

def _starlark_apple_static_library_impl(ctx):
    if not hasattr(apple_common.platform_type, ctx.attr.platform_type):
        fail('Unsupported platform type \"{}\"'.format(ctx.attr.platform_type))
    link_result = apple_common.link_multi_arch_static_library(ctx = ctx)
    processed_library = ctx.actions.declare_file(
        "{}_lipo.a".format(ctx.label.name),
    )
    files_to_build = [processed_library]
    runfiles = ctx.runfiles(
        files = files_to_build,
        collect_default = True,
        collect_data = True,
    )
    lipo_inputs = [output.library for output in link_result.outputs]
    if len(lipo_inputs) > 1:
        apple_env = {}
        xcode_config = ctx.attr._xcode_config[apple_common.XcodeVersionConfig]
        apple_env.update(apple_common.apple_host_system_env(xcode_config))
        apple_env.update(
            apple_common.target_apple_env(
                xcode_config,
                ctx.fragments.apple.single_arch_platform,
            ),
        )
        args = ctx.actions.args()
        args.add("-create")
        args.add_all(lipo_inputs)
        args.add("-output", processed_library)
        ctx.actions.run(
            arguments = [args],
            env = apple_env,
            executable = "/usr/bin/lipo",
            execution_requirements = xcode_config.execution_info(),
            inputs = lipo_inputs,
            outputs = [processed_library],
        )
    else:
        ctx.actions.symlink(
            target_file = lipo_inputs[0],
            output = processed_library,
        )
    providers = [
        DefaultInfo(files = depset(files_to_build), runfiles = runfiles),
        link_result.output_groups,
    ]
    if getattr(link_result, "objc", None):
        providers.append(link_result.objc)
    return providers

starlark_apple_static_library = rule(
    _starlark_apple_static_library_impl,
    attrs = {
        "_child_configuration_dummy": attr.label(
            cfg = apple_platform_split_transition,
            default = Label("@bazel_tools//tools/cpp:current_cc_toolchain"),
        ),
        "_xcode_config": attr.label(
            default = configuration_field(
                fragment = "apple",
                name = "xcode_config_label",
            ),
        ),
        "_xcrunwrapper": attr.label(
            executable = True,
            cfg = "exec",
            default = Label("@bazel_tools//tools/objc:xcrunwrapper"),
        ),
        "additional_linker_inputs": attr.label_list(
            allow_files = True,
        ),
        "avoid_deps": attr.label_list(
            cfg = apple_platform_split_transition,
            default = [],
        ),
        "deps": attr.label_list(
            cfg = apple_platform_split_transition,
        ),
        "linkopts": attr.string_list(),
        "platform_type": attr.string(mandatory = True),
        "minimum_os_version": attr.string(mandatory = True),
        "_allowlist_function_transition": attr.label(
            default = "@bazel_tools//tools/allowlists/function_transition_allowlist",
        ),
    },
    fragments = ["apple", "objc", "cpp"],
)
