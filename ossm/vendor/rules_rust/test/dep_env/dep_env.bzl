"""Tests for passing configuration to cargo_dep_env rules"""

_SH_WRITER = """\
#!/usr/bin/env bash
set -euo pipefail
echo content > $@
"""

_BAT_WRITER = """\
@ECHO OFF
echo content > %*
"""

def _create_dep_dir_impl(ctx):
    is_windows = ctx.attr.platform.values()[0] == "windows"
    writer = ctx.actions.declare_file("{}.writer.{}".format(
        ctx.label.name,
        ".bat" if is_windows else ".sh",
    ))
    ctx.actions.write(
        output = writer,
        content = _BAT_WRITER if is_windows else _SH_WRITER,
        is_executable = True,
    )

    out = ctx.actions.declare_directory("{}.dep_dir".format(ctx.label.name))
    ctx.actions.run(
        executable = writer,
        outputs = [out],
        arguments = [
            "{}\\a_file".format(out.path) if is_windows else "{}/a_file".format(out.path),
        ],
    )

    return [DefaultInfo(files = depset(direct = [out]))]

_create_dep_dir = rule(
    implementation = _create_dep_dir_impl,
    attrs = {
        "platform": attr.label_keyed_string_dict(
            doc = "The name of the exec platform",
            cfg = "exec",
            mandatory = True,
        ),
    },
)

def create_dep_dir(name, **kwargs):
    native.filegroup(
        name = "{}_".format(name),
    )
    _create_dep_dir(
        name = name,
        platform = select({
            "@platforms//os:windows": {":{}_".format(name): "windows"},
            "//conditions:default": {":{}_".format(name): "unix"},
        }),
        **kwargs
    )
