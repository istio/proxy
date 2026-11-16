"""Toolchain for hermetic pigz (parallel gzip)."""

_TOOLCHAIN_TYPE = Label("//pigz:toolchain_type")

PigzInfo = provider(
    doc = "Information about a pigz implementation.",
    fields = {
        "pigz": "File: the `pigz` executable, or None if there is no pigz for the platform.",
        "runfiles": "runfiles needed to run `pigz`.",
        "unavailable": "str: why there is no pigz for the platform (when `pigz` is None).",
    },
)

UNAVAILABLE_MESSAGE = (
    "No pigz for this platform: toolshed's prebuilt pigz is for Linux x86_64 and aarch64 " +
    "(once a bins release publishes them), and no other pigz toolchain is registered. " +
    "Set a host pigz (`pigz_prebuilt_extension`'s `setup(host_path = ...)`), or register a " +
    "source toolchain (see @envoy_toolshed//pigz:README.md)."
)

def _pigz_toolchain_impl(ctx):
    executable = ctx.attr.pigz[DefaultInfo].files_to_run.executable
    if not executable:
        fail("`pigz` (%s) does not provide an executable" % ctx.attr.pigz.label)
    runfiles = ctx.runfiles(files = [executable])
    runfiles = runfiles.merge(ctx.attr.pigz[DefaultInfo].default_runfiles)
    return [
        platform_common.ToolchainInfo(
            pigz = PigzInfo(
                pigz = executable,
                runfiles = runfiles,
                unavailable = None,
            ),
        ),
        platform_common.TemplateVariableInfo({"PIGZ": executable.path}),
    ]

pigz_toolchain = rule(
    implementation = _pigz_toolchain_impl,
    doc = """Declares a pigz implementation for `//pigz:toolchain_type`.

The executable is built for (or is a prebuilt of) the platform that the toolchain's
`target_compatible_with` matches: the toolchain is consumed through `//pigz`, as a tool in
the exec configuration, where the target platform is the one the tool runs on.""",
    attrs = {
        "pigz": attr.label(
            cfg = "target",
            doc = "Executable implementing `pigz`.",
            executable = True,
            mandatory = True,
        ),
    },
)

def _pigz_unavailable_impl(_ctx):
    return [platform_common.ToolchainInfo(
        pigz = PigzInfo(
            pigz = None,
            runfiles = None,
            unavailable = UNAVAILABLE_MESSAGE,
        ),
    )]

pigz_unavailable = rule(
    implementation = _pigz_unavailable_impl,
    doc = """A `//pigz:toolchain_type` implementation for platforms without a pigz.

Registered after every other pigz toolchain, so that a platform without a prebuilt, or a source
toolchain of the consumer's, fails with a message (from `//pigz`) rather than building pigz or
an unmatched toolchain error.""",
)

def _resolved_pigz_impl(ctx):
    info = ctx.toolchains[_TOOLCHAIN_TYPE].pigz
    if not info.pigz:
        fail(info.unavailable)
    executable = ctx.actions.declare_file("%s_bin/pigz" % ctx.label.name)
    ctx.actions.symlink(
        output = executable,
        target_file = info.pigz,
        is_executable = True,
    )
    return [DefaultInfo(
        executable = executable,
        files = depset([executable]),
        runfiles = info.runfiles.merge(ctx.runfiles(files = [executable])),
    )]

resolved_pigz = rule(
    implementation = _resolved_pigz_impl,
    doc = """The `pigz` of the resolved `//pigz:toolchain_type`, as an executable target.

Use it as a tool (`cfg = "exec"`), eg as the `compressor` of a `pkg_tar`, so that it is resolved
for the platform the action runs on.""",
    executable = True,
    toolchains = [str(_TOOLCHAIN_TYPE)],
)
