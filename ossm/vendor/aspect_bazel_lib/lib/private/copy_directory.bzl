"""Implementation of copy_directory macro and underlying rules.

This rule copies a directory to another location using a precompiled binary.
"""

load(":copy_common.bzl", _COPY_EXECUTION_REQUIREMENTS = "COPY_EXECUTION_REQUIREMENTS")

def copy_directory_bin_action(
        ctx,
        src,
        dst,
        copy_directory_bin,
        copy_directory_toolchain = "@aspect_bazel_lib//lib:copy_directory_toolchain_type",
        hardlink = "auto",
        verbose = False,
        preserve_mtime = False):
    """Factory function that creates an action to copy a directory from src to dst using a tool binary.

    The tool binary will typically be the `@aspect_bazel_lib//tools/copy_directory` `go_binary`
    either built from source or provided by a toolchain.

    This helper is used by the copy_directory rule. It is exposed as a public API so it can be used
    within other rule implementations.

    Args:
        ctx: The rule context.

        src: The source directory to copy.

        dst: The directory to copy to. Must be a TreeArtifact.

        copy_directory_bin: Copy to directory tool binary.

        copy_directory_toolchain: The toolchain type for Auto Exec Groups. The default is probably what you want.

        hardlink: Controls when to use hardlinks to files instead of making copies.

            See copy_directory rule documentation for more details.

        verbose: print verbose logs to stdout

        preserve_mtime: preserve the modified time from the source.
            See the caveats above about interactions with remote execution and caching.

    """
    args = [
        src.path,
        dst.path,
    ]
    if verbose:
        args.append("--verbose")

    if hardlink == "on":
        args.append("--hardlink")
    elif hardlink == "auto" and not src.is_source:
        args.append("--hardlink")

    if preserve_mtime:
        args.append("--preserve-mtime")

    ctx.actions.run(
        inputs = [src],
        outputs = [dst],
        executable = copy_directory_bin,
        arguments = args,
        mnemonic = "CopyDirectory",
        progress_message = "Copying directory %{input}",
        execution_requirements = _COPY_EXECUTION_REQUIREMENTS,
        toolchain = copy_directory_toolchain,
    )

def _copy_directory_impl(ctx):
    copy_directory_bin = ctx.toolchains["@aspect_bazel_lib//lib:copy_directory_toolchain_type"].copy_directory_info.bin

    dst = ctx.actions.declare_directory(ctx.attr.out)

    copy_directory_bin_action(
        ctx,
        src = ctx.file.src,
        dst = dst,
        # copy_directory_bin = ctx.executable._tool,  # use for development
        copy_directory_bin = copy_directory_bin,
        hardlink = ctx.attr.hardlink,
        verbose = ctx.attr.verbose,
        preserve_mtime = ctx.attr.preserve_mtime,
    )

    return [
        DefaultInfo(
            files = depset([dst]),
            runfiles = ctx.runfiles([dst]),
        ),
    ]

_copy_directory = rule(
    implementation = _copy_directory_impl,
    provides = [DefaultInfo],
    attrs = {
        "src": attr.label(mandatory = True, allow_single_file = True),
        # Cannot declare out as an output here, because there's no API for declaring
        # TreeArtifact outputs.
        "out": attr.string(mandatory = True),
        "hardlink": attr.string(
            values = ["auto", "off", "on"],
            default = "auto",
        ),
        "verbose": attr.bool(),
        "preserve_mtime": attr.bool(
            doc = """If True, the last modified time of copied files is preserved. Note the caveats on copy_directory.""",
            default = False,
        ),
        # use '_tool' attribute for development only; do not commit with this attribute active since it
        # propagates a dependency on rules_go which would be breaking for users
        # "_tool": attr.label(
        #     executable = True,
        #     cfg = "exec",
        #     default = "//tools/copy_directory",
        # ),
    },
    toolchains = ["@aspect_bazel_lib//lib:copy_directory_toolchain_type"],
)

def copy_directory(
        name,
        src,
        out,
        hardlink = "auto",
        **kwargs):
    """Copies a directory to another location.

    This rule uses a precompiled binary to perform the copy, so no shell is required.

    If using this rule with source directories, it is recommended that you use the
    `--host_jvm_args=-DBAZEL_TRACK_SOURCE_DIRECTORIES=1` startup option so that changes
    to files within source directories are detected. See
    https://github.com/bazelbuild/bazel/commit/c64421bc35214f0414e4f4226cc953e8c55fa0d2
    for more context.

    Args:
      name: Name of the rule.

      src: The directory to make a copy of. Can be a source directory or TreeArtifact.

      out: Path of the output directory, relative to this package.

      hardlink: Controls when to use hardlinks to files instead of making copies.

        Creating hardlinks is much faster than making copies of files with the caveat that
        hardlinks share file permissions with their source.

        Since Bazel removes write permissions on files in the output tree after an action completes,
        hardlinks to source files within source directories is not recommended since write
        permissions will be inadvertently removed from sources files.

        - "auto": hardlinks are used if src is a tree artifact already in the output tree
        - "off": files are always copied
        - "on": hardlinks are always used (not recommended)

      **kwargs: further keyword arguments, e.g. `visibility`
    """
    _copy_directory(
        name = name,
        src = src,
        out = out,
        hardlink = hardlink,
        **kwargs
    )
