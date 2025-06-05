load(
    "@rules_foreign_cc//foreign_cc/private/framework/toolchains:linux_commands.bzl",
    "commands"
)

def _foreign_cc_framework_toolchain_impl(ctx):
    return platform_common.ToolchainInfo(
        commands = commands,
    )

foreign_cc_framework_toolchain = rule(
    doc = "A toolchain contianing foreign_cc commands",
    implementation = _foreign_cc_framework_toolchain_impl,
)
