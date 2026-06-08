"""
The platform_data rule can be used to change the target platform of a target,
and then depend on that elsewhere in the build tree. For example:
load("@platforms//experimental/platform_data:defs.bzl", "platform_data")

cc_binary(name = "foo")

platform_data(
    name = "foo_embedded",
    target = ":foo",
    platform = "//my/new:platform",
)

py_binary(
    name = "flasher",
    srcs = ...,
    data = [
        ":foo_embedded",
    ],
)

Regardless of what platform the top-level :flasher binary is built for,
the :foo_embedded target will be built for //my/new:platform. 

Note that if you depend on :foo_embedded it's not exactly the same as depending on :foo, since it won't forward all the same providers. In the future, we can extend this to add some common providers as needed."""

def _target_platform_transition_impl(attr):
    return {
        "//command_line_option:platforms": str(attr.platform),
    }

_target_platform_transition = transition(
    implementation = _target_platform_transition_impl,
    inputs = [],
    outputs = [
        "//command_line_option:platforms",
    ],
)

def _platform_data_impl(ctx):
    target = ctx.attr.target

    default_info = target[DefaultInfo]
    files = default_info.files
    original_executable = default_info.files_to_run.executable
    runfiles = default_info.default_runfiles

    new_executable = ctx.actions.declare_file(ctx.attr.name)

    ctx.actions.symlink(
        output = new_executable,
        target_file = original_executable,
        is_executable = True,
    )

    files = depset(direct = [new_executable], transitive = [files])
    runfiles = runfiles.merge(ctx.runfiles([new_executable]))

    return [
	DefaultInfo(
            files = files,
            runfiles = runfiles,
            executable = new_executable,
	)
    ]

platform_data = rule(
    implementation = _platform_data_impl,
    attrs = {
        "target": attr.label(
            allow_files = False,
            executable = True,
            mandatory = True,
            cfg = _target_platform_transition,
        ),
        "platform": attr.label(
            mandatory = True,
        ),
        "_allowlist_function_transition": attr.label(
            default = "@bazel_tools//tools/allowlists/function_transition_allowlist",
        ),
    },
    executable = True,
)

