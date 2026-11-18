"""Utilities for detecting Windows execution platform."""

def _is_exec_platform_windows(ctx):
    is_windows = ctx.target_platform_has_constraint(ctx.attr._windows_constraint[platform_common.ConstraintValueInfo])
    executable = ctx.actions.declare_file("windows_exec.bats")
    ctx.actions.write(
        executable,
        content = "@noop",
    )

    return [
        DefaultInfo(executable = executable),
        OutputGroupInfo(windows = depset()) if is_windows else OutputGroupInfo(),
    ]

is_exec_platform_windows = rule(
    implementation = _is_exec_platform_windows,
    attrs = {
        "_windows_constraint": attr.label(default = Label("@platforms//os:windows")),
    },
)

IS_EXEC_PLATFORM_WINDOWS_ATTRS = {
    "_is_platform_windows_exec": attr.label(
        default = Label("//oci/private:is_exec_platform_windows"),
        executable = True,
        cfg = "exec",
    ),
}

def is_windows_exec(ctx):
    """Utility function for checking if the action run on windows.

    TODO: explain

    Args:
        ctx: rule context
    """

    outputgroupinfo = ctx.attr._is_platform_windows_exec[OutputGroupInfo]
    return hasattr(outputgroupinfo, "windows")
