"""Toolchain and helper definitions for esbuild"""

def _esbuild_toolchain_impl(ctx):
    return [
        platform_common.ToolchainInfo(
            binary = ctx.executable.binary,
        ),
        platform_common.TemplateVariableInfo({
            "ESBUILD_PATH": ctx.executable.binary.path,
        }),
    ]

_esbuild_toolchain = rule(
    implementation = _esbuild_toolchain_impl,
    attrs = {
        "binary": attr.label(
            allow_single_file = True,
            executable = True,
            cfg = "exec",
        ),
    },
)

TOOLCHAIN = Label("@build_bazel_rules_nodejs//toolchains/esbuild:toolchain_type")

def configure_esbuild_toolchain(name, binary, exec_compatible_with):
    """Defines a toolchain for esbuild given the binary path and platform constraints

    Args:
        name: unique name for this toolchain, generally in the form "esbuild_platform_arch"
        binary: label for the esbuild binary
        exec_compatible_with: list of platform constraints
    """

    _esbuild_toolchain(
        name = name,
        binary = binary,
    )

    native.toolchain(
        name = "%s_toolchain" % name,
        exec_compatible_with = exec_compatible_with,
        toolchain = name,
        toolchain_type = TOOLCHAIN,
    )

def configure_esbuild_toolchains(name = "", platforms = {}):
    """Configures esbuild toolchains for a list of supported platforms

    Args:
        name: unused
        platforms: dict of platforms to configure toolchains for
    """

    for name, meta in platforms.items():
        repo = "esbuild_%s" % name
        configure_esbuild_toolchain(
            name = repo,
            binary = "@%s//:%s" % (repo, meta.binary_path),
            exec_compatible_with = meta.exec_compatible_with,
        )
