"""Toolchain definitions for rules_proto_grpc."""

def _protoc_toolchain_impl(ctx):
    return [platform_common.ToolchainInfo(
        protoc_target = ctx.attr.protoc,
        protoc_executable = ctx.executable.protoc,
        fixer_executable = ctx.executable.fixer,
    )]

protoc_toolchain = rule(
    implementation = _protoc_toolchain_impl,
    attrs = {
        "protoc": attr.label(
            doc = "The protocol compiler tool",
            default = "@com_google_protobuf//:protoc",
            executable = True,
            cfg = "exec",
        ),
        "fixer": attr.label(
            doc = "The fixer tool",
            default = "//tools/fixer",
            executable = True,
            cfg = "exec",
        ),
    },
    provides = [platform_common.ToolchainInfo],
)
