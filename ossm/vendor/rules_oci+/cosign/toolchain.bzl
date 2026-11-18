"""This module implements the cosign-specific toolchain rule."""

CosignInfo = provider(
    doc = "Information about how to invoke the cosign executable.",
    fields = {
        "binary": "Executable cosign binary",
    },
)

def _cosign_toolchain_impl(ctx):
    binary = ctx.executable.cosign

    # Make the $(COSIGN_BIN) variable available in places like genrules.
    # See https://docs.bazel.build/versions/main/be/make-variables.html#custom_variables
    template_variables = platform_common.TemplateVariableInfo({
        "COSIGN_BIN": binary.path,
    })
    default = DefaultInfo(
        files = depset([binary]),
        runfiles = ctx.runfiles(files = [binary]),
    )
    cosign_info = CosignInfo(binary = binary)

    # Export all the providers inside our ToolchainInfo
    # so the resolved_toolchain rule can grab and re-export them.
    toolchain_info = platform_common.ToolchainInfo(
        cosign_info = cosign_info,
        template_variables = template_variables,
        default = default,
    )
    return [
        default,
        toolchain_info,
        template_variables,
    ]

cosign_toolchain = rule(
    implementation = _cosign_toolchain_impl,
    attrs = {
        "cosign": attr.label(
            doc = "A hermetically downloaded cosign executable target for the target platform.",
            mandatory = True,
            allow_single_file = True,
            executable = True,
            cfg = "exec",
        ),
    },
    doc = """Defines a cosign toolchain.

For usage see https://docs.bazel.build/versions/main/toolchains.html#defining-toolchains.
""",
)
