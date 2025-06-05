
def _buf_toolchain_impl(ctx):
    toolchain_info = platform_common.ToolchainInfo(
        cli = ctx.executable.cli,
    )
    return [toolchain_info]

_buf_toolchain = rule(
    implementation = _buf_toolchain_impl,
    attrs = {
        "cli": attr.label(
            doc = "The buf cli",
            executable = True,
            allow_single_file = True,
            mandatory = True,
            cfg = "exec",
        ),
    },
)

def declare_buf_toolchains(os, cpu, rules_buf_repo_name):
    for cmd in ["buf", "protoc-gen-buf-lint", "protoc-gen-buf-breaking"]:
        ext = ""
        if os == "windows":
            ext = ".exe"
        toolchain_impl = cmd + "_toolchain_impl"
        _buf_toolchain(
            name = toolchain_impl,
            cli = str(Label("//:"+ cmd)),
        )
        native.toolchain(
            name = cmd + "_toolchain",
            toolchain = ":" + toolchain_impl,
            toolchain_type = "@@{}//tools/{}:toolchain_type".format(rules_buf_repo_name, cmd),
            exec_compatible_with = [
                "@platforms//os:" + os,
                "@platforms//cpu:" + cpu,
            ],
        )

