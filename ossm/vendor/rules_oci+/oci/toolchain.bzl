"""This module implements the language-specific toolchain rule."""

CraneInfo = provider(
    doc = "Information about how to invoke the crane executable.",
    fields = {
        "binary": "Executable crane binary",
        "version": "Crane version",
    },
)

def _crane_toolchain_impl(ctx):
    binary = ctx.executable.crane
    template_variables = platform_common.TemplateVariableInfo({
        "CRANE_BIN": binary.path,
    })
    default = DefaultInfo(
        files = depset([binary]),
        runfiles = ctx.runfiles(files = [binary]),
    )
    crane_info = CraneInfo(
        binary = binary,
        version = ctx.attr.version.removeprefix("v"),
    )
    toolchain_info = platform_common.ToolchainInfo(
        crane_info = crane_info,
        template_variables = template_variables,
        default = default,
    )
    return [
        default,
        toolchain_info,
        template_variables,
    ]

crane_toolchain = rule(
    implementation = _crane_toolchain_impl,
    attrs = {
        "crane": attr.label(
            doc = "A hermetically downloaded executable target for the target platform.",
            mandatory = True,
            allow_single_file = True,
            executable = True,
            cfg = "exec",
        ),
        "version": attr.string(mandatory = True, doc = "Version of the crane binary"),
    },
    doc = "Defines a crane toolchain. See: https://docs.bazel.build/versions/main/toolchains.html#defining-toolchains.",
)

RegCtlInfo = provider(
    doc = "Information about how to invoke the regctl executable.",
    fields = {
        "binary": "Executable regctl binary",
    },
)

def _regctl_toolchain_impl(ctx):
    binary = ctx.executable.regctl
    template_variables = platform_common.TemplateVariableInfo({
        "REGCTL_BIN": binary.path,
    })
    default = DefaultInfo(
        files = depset([binary]),
        runfiles = ctx.runfiles(files = [binary]),
    )
    regctl_info = RegCtlInfo(
        binary = binary,
    )
    toolchain_info = platform_common.ToolchainInfo(
        regctl_info = regctl_info,
        template_variables = template_variables,
        default = default,
    )
    return [
        default,
        toolchain_info,
        template_variables,
    ]

regctl_toolchain = rule(
    implementation = _regctl_toolchain_impl,
    attrs = {
        "regctl": attr.label(
            doc = "A hermetically downloaded executable target for the target platform.",
            mandatory = True,
            allow_single_file = True,
            executable = True,
            cfg = "exec",
        ),
    },
    doc = "Defines a regctl toolchain. See: https://docs.bazel.build/versions/main/toolchains.html#defining-toolchains.",
)
