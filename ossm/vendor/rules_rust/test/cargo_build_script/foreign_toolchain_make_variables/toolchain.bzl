"""Utilties for testing forwarding Make variables from toolchains."""

def _dummy_env_var_toolchain_impl(_ctx):
    make_variables = platform_common.TemplateVariableInfo({
        "ALSO_FROM_TOOLCHAIN": "absent",
        "FROM_TOOLCHAIN": "present",
    })

    return [
        platform_common.ToolchainInfo(
            make_variables = make_variables,
        ),
        make_variables,
    ]

dummy_env_var_toolchain = rule(
    implementation = _dummy_env_var_toolchain_impl,
)
