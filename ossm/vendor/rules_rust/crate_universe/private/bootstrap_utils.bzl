"""Utilities directly related to bootstrapping `cargo-bazel`"""

_SRCS_TEMPLATE = """\
\"\"\"A generate file containing all source files used to produce `cargo-bazel`\"\"\"

# Each source file is tracked as a target so the `cargo_bootstrap_repository`
# rule will know to automatically rebuild if any of the sources changed.

# Run 'bazel run //crate_universe/private:srcs_module.install' to regenerate.

CARGO_BAZEL_SRCS = [
    {srcs}
]
"""

def _format_src_label(label):
    if label.workspace_name != "":
        fail("`srcs` must be from the rules_rust repository")
    return "Label(\"{}\"),".format(str(label).lstrip("@"))

def _srcs_module_impl(ctx):
    srcs = [_format_src_label(src.owner) for src in sorted(ctx.files.srcs)]
    if not srcs:
        fail("`srcs` cannot be empty")
    output = ctx.actions.declare_file(ctx.label.name)

    ctx.actions.write(
        output = output,
        content = _SRCS_TEMPLATE.format(
            srcs = "\n    ".join(srcs),
        ),
    )

    return DefaultInfo(
        files = depset([output]),
    )

_srcs_module = rule(
    doc = "A rule for writing a list of sources to a templated file",
    implementation = _srcs_module_impl,
    attrs = {
        "srcs": attr.label(
            doc = "A filegroup of source files",
            allow_files = True,
        ),
    },
)

_INSTALLER_TEMPLATE = """\
#!/usr/bin/env bash
set -euo pipefail
cp -f "{path}" "${{BUILD_WORKSPACE_DIRECTORY}}/{dest}"
"""

def _srcs_installer_impl(ctx):
    output = ctx.actions.declare_file(ctx.label.name + ".sh")
    target_file = ctx.file.input
    dest = ctx.file.dest.short_path

    ctx.actions.write(
        output = output,
        content = _INSTALLER_TEMPLATE.format(
            path = target_file.short_path,
            dest = dest,
        ),
        is_executable = True,
    )

    return DefaultInfo(
        files = depset([output]),
        runfiles = ctx.runfiles(files = [target_file]),
        executable = output,
    )

_srcs_installer = rule(
    doc = "A rule for writing a file back to the repository",
    implementation = _srcs_installer_impl,
    attrs = {
        "dest": attr.label(
            doc = "the file name to use for installation",
            allow_single_file = True,
            mandatory = True,
        ),
        "input": attr.label(
            doc = "The file to write back to the repository",
            allow_single_file = True,
            mandatory = True,
        ),
    },
    executable = True,
)

def srcs_module(name, dest, **kwargs):
    """A helper rule to ensure the bootstrapping functionality of `cargo-bazel` is always up to date

    Args:
        name (str): The name of the sources module
        dest (str): The filename the module should be written as in the current package.
        **kwargs (dict): Additional keyword arguments
    """
    tags = kwargs.pop("tags", [])

    _srcs_module(
        name = name,
        tags = tags,
        **kwargs
    )

    _srcs_installer(
        name = name + ".install",
        input = name,
        dest = dest,
        tags = tags,
    )
