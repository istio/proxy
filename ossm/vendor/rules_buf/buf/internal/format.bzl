_DOC = """
buf_format rule formats Protobuf files.
"""

_TEMPLATE = """
buf=$(readlink "{}")
if ! cd "$BUILD_WORKSPACE_DIRECTORY"; then
  echo "Unable to change to workspace (BUILD_WORKSPACE_DIRECTORY: $BUILD_WORKSPACE_DIRECTORY)"
  exit 1
fi

$buf format {} .
"""

_TOOLCHAIN = str(Label("//tools/buf:toolchain_type"))

def _buf_format_impl(ctx):
    ctx.actions.write(
        output = ctx.outputs.executable,
        is_executable = True,
        content = _TEMPLATE.format(ctx.toolchains[_TOOLCHAIN].cli.short_path, "-w"),
    )

    return [
        DefaultInfo(
            runfiles = ctx.runfiles(
                files = [ctx.toolchains[_TOOLCHAIN].cli],
            ),
        ),
    ]

buf_format = rule(
    implementation = _buf_format_impl,
    doc = _DOC,
    attrs = {},
    toolchains = [_TOOLCHAIN],
    executable = True,
)
